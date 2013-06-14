
-- ###########################################################################
-- GeneratorJSON - generate a rules.conf or similar
-- ###########################################################################
local ibjson = require('ibjson')
local Generator = require('ironbee/waggle/generator')
local GeneratorJSON = {}
GeneratorJSON.__index = GeneratorJSON
setmetatable(GeneratorJSON, Generator)
GeneratorJSON.type = 'generatorjson'
GeneratorJSON.new = function(self)
    local t = {}
    return setmetatable(t, self)
end

GeneratorJSON.generate = function(self, plan, db)
    local j = {}

    for _, chain in ipairs(plan) do

        for i, link in ipairs(chain) do
            local rule_id = link.rule
            local result = link.result
            local rule = db.db[rule_id]

            local t = {
                comment = rule.data.comment,
                operator = self:gen_op(rule),
                operator_argument = rule.data.op_arg,
                rule_type = rule.data.rule_type,
                tags = {},
                actions = {},
                fields = {
                }
            }

            -- For external rules, there is a script field.
            if rule.data.script then
                t.script = rule.data.script
            end

            if rule.data.message then
                table.insert(
                    t.actions,
                    { name = 'msg', argument = rule.data.message })
            end

            for tag, _ in pairs(rule.data.tags) do
                table.insert(t.tags, tag)
            end

            for _, act in ipairs(rule.data.actions) do 
                table.insert(t.actions, act)
            end

            for _, field in ipairs(rule.data.fields) do 
                table.insert(t.fields, {
                    collection = field.collection,
                    selector = field.selector,
                    transformation = field.transformation
                })
            end

            table.insert(j, t)

            -- The first rule in a chain gets the ID, message, phase, etc.
            if i == 1 then
                local last_rule = db.db[chain[#chain].rule]
                table.insert(
                    t.actions,
                    { name = 'id', argument = rule.data.id })
                table.insert(
                    t.actions, 
                    { name = 'phase', argument = last_rule.data.phase })
                table.insert(
                    t.actions, 
                    { name = 'rev', argument = last_rule.data.version })
            end

            if i ~= #chain then
                table.insert(t.actions, { name = "chain", argument = nil })
            end
        end
    end

    return ibjson.to_string(j)
end

return GeneratorJSON
