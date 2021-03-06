/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief Predicate --- Standard Template Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_template.hpp>

#include <predicate/bfs.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/tree_copy.hpp>
#include <predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

string Ref::name() const
{
    return "ref";
}

void Ref::calculate(EvalContext context)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "Ref node calculated.  "
            "Maybe transform skipped; or used outside of template body."
        )
    );
}

void Ref::post_transform(NodeReporter reporter) const
{
    reporter.error(
        "Ref node should not exist post-transform.  "
        "Maybe used outside of template body."
    );
}

bool Ref::validate(NodeReporter reporter) const
{
    bool result =
        Validate::n_children(reporter, 1) &&
        Validate::nth_child_is_string(reporter, 0)
        ;
    if (result) {
        string ref_param =
            literal_value(children().front())
            .value_as_byte_string()
            .to_s()
            ;
        if (
            ref_param.empty() ||
            ref_param.find_first_not_of(
                "_0123456789"
                "abcdefghijklmnoprstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            ) != string::npos
        ) {
            reporter.error(
                "Reference parameter \"" + ref_param +
                "\" is not legal."
            );
            result = false;
        }
    }
    return result;
}

Template::Template(
    const string&              name,
    const template_arg_list_t& args,
    const node_cp&             body
) :
    m_name(name),
    m_args(args),
    m_body(body)
{
    // nop
}

string Template::name() const
{
    return m_name;
}

void Template::calculate(EvalContext context)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "Template node calculated.  "
            "Must have skipped transform."
        )
    );
}

void Template::post_transform(NodeReporter reporter) const
{
    reporter.error(
        "Template node should not exist post-transform."
    );
}

bool Template::validate(NodeReporter reporter) const
{
    return
        Validate::n_children(reporter, m_args.size())
        ;
}

namespace {

string template_ref(const node_cp& node)
{
    const Ref* as_ref = dynamic_cast<const Ref*>(node.get());
    if (! as_ref) {
        return string();
    }

    return literal_value(as_ref->children().front())
        .value_as_byte_string()
        .to_s()
        ;
}

}

bool Template::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    // Construct map of argument name to children.
    typedef map<string, node_p> arg_map_t;
    arg_map_t arg_map;

    {
        template_arg_list_t::const_iterator arg_i = m_args.begin();
        node_list_t::const_iterator children_i = children().begin();

        while (arg_i != m_args.end() && children_i != children().end()) {
            arg_map.insert(make_pair(*arg_i, *children_i));
            ++arg_i;
            ++children_i;
        }

        if (arg_i != m_args.end() || children_i != children().end()) {
            reporter.error(
                "Number of children not equal to number of arguments.  "
                "Should have been caught in validation."
            );
            return false;
        }
    }

    // Construct copy of body to replace me with.
    node_p replacement = tree_copy(m_body, call_factory);

    // Special case.  Body might be itself a ref node.
    {
        string top_ref = template_ref(m_body);
        if (! top_ref.empty()) {
            arg_map_t::const_iterator arg_i = arg_map.find(top_ref);
            if (arg_i == arg_map.end()) {
                reporter.error(
                    "Reference to \"" + top_ref + "\" but not such "
                    "argument to template " + name() = "."
                );
                return false;
            }

            node_p replacement = arg_i->second;
            merge_graph.replace(me, replacement);
            return true;
        }
    }

    // Make list of all descendants.  We don't want to iterate over the
    // replacements, so we make the entire list in advance.
    list<node_p> to_transform;
    bfs_down(replacement, back_inserter(to_transform));
    BOOST_FOREACH(const node_p& node, to_transform) {
        BOOST_FOREACH(const node_p& child, node->children()) {
            string ref_param = template_ref(child);
            if (! ref_param.empty()) {
                arg_map_t::const_iterator arg_i = arg_map.find(ref_param);
                if (arg_i == arg_map.end()) {
                    reporter.error(
                        "Reference to \"" + ref_param + "\" but not such "
                        "argument to template " + name() = "."
                    );
                    continue;
                }

                node->replace_child(child, arg_i->second);
            }
        }
    }

    // Replace me.
    merge_graph.replace(me, replacement);

    return true;
}

namespace {

call_p define_template_creator(
    const std::string&        name,
    const template_arg_list_t args,
    const node_cp             body
)
{
    return call_p(new Template(name, args, body));
}

}

CallFactory::generator_t define_template(
    const template_arg_list_t& args,
    const node_cp&             body
)
{
    return bind(define_template_creator, _1, args, body);
}

void load_template(CallFactory& to)
{
    to
        .add<Ref>()
        ;
}

} // Standard
} // Predicate
} // IronBee
