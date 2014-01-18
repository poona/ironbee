class TestInitCollection < Test::Unit::TestCase
  include CLIPPTest

  def test_init_collection_load
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :config => '''
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_init_collection.so
      ''',
      :default_site_config => ''
    )

    assert_no_issues
  end

  def test_init_collection_var
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :config => '''
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_init_collection.so

        InitCollection COL1 vars: \
          A=a1 \
          B=b1
        InitCollection COL2 vars: \
          A=a2 \
          B=b2
      ''',
      :default_site_config => <<-EOS
        Rule COL1:A @clipp_print 'A1' id:1 rev:1 phase:REQUEST
        Rule COL1:B @clipp_print 'B1' id:2 rev:1 phase:REQUEST
        Rule COL2:A @clipp_print 'A2' id:3 rev:1 phase:REQUEST
        Rule COL2:B @clipp_print 'B2' id:4 rev:1 phase:REQUEST
      EOS
    )

    assert_no_issues
    assert_log_match /clipp_print \['A1'\]: a1/
    assert_log_match /clipp_print \['B1'\]: b1/
    assert_log_match /clipp_print \['A2'\]: a2/
    assert_log_match /clipp_print \['B2'\]: b2/
  end

  def test_init_collection_json
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :config => '''
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_init_collection.so

        InitCollection COL1 json-file://init_collection_1.json \
          A=a1 \
          B=b1
        InitCollection COL2 json-file://init_collection_2.json \
          A=a2 \
          B=b2
      ''',
      :default_site_config => <<-EOS
        Rule COL1:A @clipp_print 'A1' id:1 rev:1 phase:REQUEST
        Rule COL1:B @clipp_print 'B1' id:2 rev:1 phase:REQUEST
        Rule COL2:A @clipp_print 'A2' id:3 rev:1 phase:REQUEST
        Rule COL2:B @clipp_print 'B2' id:4 rev:1 phase:REQUEST
      EOS
    )

    assert_no_issues
    assert_log_match /clipp_print \['A1'\]: a1/
    assert_log_match /clipp_print \['B1'\]: b1/
    assert_log_match /clipp_print \['A2'\]: a2/
    assert_log_match /clipp_print \['B2'\]: b2/
  end

  def test_init_collection_two_sites
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :config => '''
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_init_collection.so
        LogLevel DEBUG

        <Site s1>
          SiteId 57f2b6d0-7783-012f-86c6-001f5b320164
          Hostname s1

          InitCollection COL1 vars: A=a1 B=b1
          InitCollection COL2 vars: A=a2 B=b2
        </Site>

        <Site s2>
          SiteId 57f2b6d0-7783-012f-86c6-001f5b320164
          Hostname s2

          InitCollection COL1 vars: A=a1 B=b1
          InitCollection COL2 vars: A=a2 B=b2
        </Site>

      ''',
      :default_site_config => ''
    )

    assert_no_issues
  end

  def test_init_collection_matching_site
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :config => '''
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_init_collection.so
        LogLevel INFO

        <Site will-not-match>
          SiteId 57f2b6d0-7783-012f-86c6-001f5b320164
          Hostname will-not-match

          InitCollection COL1 vars: \
              "A=1" \
              "B=2"
        </Site>

        <Site will-match>
          SiteId 57f2b6d0-7783-012f-86c6-001f5b320165
          Hostname *

          InitCollection COL1 vars: \
              "A=1" \
              "B=2"
        </Site>
      ''',
      :default_site_config => <<-EOS
        Rule COL1 @clipp_print 'COL1' id:1 rev:1 phase:REQUEST
      EOS
    )

    assert_no_issues
  end

  def test_init_collection_no_matching_site
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :config => '''
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_init_collection.so
        LogLevel INFO

        <Site will-not-match>
          SiteId 57f2b6d0-7783-012f-86c6-001f5b320164
          Hostname will-not-match

          InitCollection COL1 vars: \
              "A=1" \
              "B=2"
        </Site>

        <Site will-not-match2>
          SiteId 57f2b6d0-7783-012f-86c6-001f5b320164
          Hostname will-not-match2

          InitCollection COL1 vars: \
              "A=1" \
              "B=2"
        </Site>
      ''',
      :default_site_config => <<-EOS
        Rule COL1 @clipp_print 'COL1' id:1 rev:1 phase:REQUEST
      EOS
    )

    assert_no_issues
  end
end

