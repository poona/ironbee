#!/usr/bin/env ruby

# See clippscript.md for documentation.

if RUBY_VERSION.split('.')[0..1].join('.').to_f < 1.9
  raise "Requires version 0.9 or higher."
end

module ClippScript
  CONNECTION_OPENED        = 1
  CONNECTION_DATA_IN       = 2
  CONNECTION_DATA_OUT      = 3
  CONNECTION_CLOSED        = 4
  REQUEST_STARTED          = 5
  REQUEST_HEADER           = 6
  REQUEST_HEADER_FINISHED  = 7
  REQUEST_BODY             = 8
  REQUEST_FINISHED         = 9
  RESPONSE_STARTED         = 10
  RESPONSE_HEADER          = 11
  RESPONSE_HEADER_FINISHED = 12
  RESPONSE_BODY            = 13
  RESPONSE_FINISHED        = 14

  def input(options = {})
    raise "Requires block." if ! block_given?
    options = symhash(options)

    result = {
      id: options[:id] || "ClippScript",
      connection: {
        pre_transaction_event:  [],
        transaction:            [],
        post_transaction_event: []
      }
    }

    yield InputProxy.new(result)

    if result[:connection][:pre_transaction_event].empty?
      delete result[:connection][:pre_transaction_event]
    end
    if result[:connection][:post_transaction_event].empty?
      delete result[:connection][:post_transaction_event]
    end

    result
  end

  def connection(options = {})
    options = symhash(options)

    connection_options = {
      local_ip:    options[:local_ip]    || "1.2.3.4",
      local_port:  options[:local_port]  || 80,
      remote_ip:   options[:remote_ip]   || "5.6.7.8",
      remote_port: options[:remote_port] || 1234
    }

    input(id: options[:id]) do |i|
      i.connection_opened(connection_options)
      yield i
      i.connection_closed
    end
  end

  def transaction(options = {}, &b)
    options = symhash(options)

    connection(
      move(options, :id, :local_ip, :local_port, :remote_ip, :remote_port)
    ) do |c|
      c.transaction(&b)
    end
  end

  def self.eval(what, &writer)
    env = Environment.new(&writer)
    results = []
    env = Environment.new do |x|
      if writer
        writer.(x)
      else
        results << v
      end
    end

    if what.is_a?(String)
      Kernel::eval(what, env.get_binding)
    else
      env.instance_eval(&what)
    end

    if writer
      nil
    else
      results
    end
  end

  class Environment
    include ClippScript

    alias_method :cs_input, :input
    private :cs_input

    def initialize(&handler)
      raise "Block required." if ! handler
      @handler = handler
    end

    def input(options = {}, &b)
      @handler.(cs_input(options, &b))
    end

    def get_binding
      binding
    end
  end

private
  EVENT_DATA = {
    CONNECTION_OPENED   => :connection_event,
    CONNECTION_DATA_IN  => :data_event,
    CONNECTION_DATA_OUT => :data_event,
    REQUEST_STARTED     => :request_event,
    REQUEST_HEADER      => :header_event,
    REQUEST_BODY        => :data_event,
    RESPONSE_STARTED    => :response_event,
    RESPONSE_HEADER     => :header_event,
    RESPONSE_BODY       => :data_event
  }

  def make_event(which, options = {})
    options = symhash(options)
    result = {
      which: which
    }
    pre_delay = options.delete(:pre_delay)
    result[:pre_delay] = pre_delay if pre_delay
    post_delay = options.delete(:post_delay)
    result[:post_delay] = post_delay if post_delay

    data_key = EVENT_DATA[which]
    if data_key
      result[data_key] = options
    end

    result
  end

  # Convert all keys to symbols.
  def symhash(hash)
    result = {}
    hash.each do |k,v|
      result[k.to_sym] = v
    end
    result
  end

  # Remove keys from from and return hash of those key-values.
  def move(from, *keys)
    r = {}
    keys.each do |k|
      v = from.delete(k)
      r[k] = v if v
    end
    r
  end

  # Provided to blocks by #input and #connection.
  class InputProxy
    include ClippScript

    def initialize(result)
      @result = result
    end

    def pre_event(which, options = {})
      @result[:connection][:pre_transaction_event] << make_event(which, options)
    end

    def post_event(which, options = {})
      @result[:connection][:post_transaction_event] << make_event(which, options)
    end

    def connection_opened(options = {})
      if ! @result[:connection][:pre_transaction_event].empty?
        raise "Multiple connection opened events."
      end

      pre_event(CONNECTION_OPENED, options)
    end

    def connection_closed(options = {})
      if ! @result[:connection][:post_transaction_event].empty?
        raise "Multiple connection closed events."
      end

      post_event(CONNECTION_CLOSED, options = {})
    end

    def transaction
      raise "Block required." if ! block_given?

      tx = []
      yield TransactionProxy.new(tx)
      @result[:connection][:transaction] << {event: tx}
    end
  end

  # Provided to blocks by InputProxy#transaction.
  class TransactionProxy
    include ClippScript

    def initialize(events)
      @events = events
    end

    def request_started(options = {})
      event(REQUEST_STARTED, options)
    end

    def request_header(options = {})
      headers = options.delete(:headers)
      options[:header] ||= []
      if headers
        options[:header].concat(headers.collect do |k, v|
          {name: k, value: v}
        end)
      end
      event(REQUEST_HEADER, options)
    end

    def request_header_finished(options = {})
      event(REQUEST_HEADER_FINISHED, options)
    end

    def headers(h)
      last_event = events.last
      if last_event && last_event[:which] == REQUEST_STARTED
        request_header(headers: h)
        request_header_finished
      elsif last_event && last_event[:which] == RESPONSE_STARTED
        response_header(headers: h)
        response_header_finished
      else
        raise "headers can only be called immediately after start of " +
              "request/response."
      end
    end

    def body(d)
      if events.last
        case events.last[:which]
        when REQUEST_HEADER_FINISHED, REQUEST_STARTED
          mode = :request
        when RESPONSE_HEADER_FINISHED, RESPONSE_STARTED
          mode = :response
        end
      end
      if ! mode
        raise "body can only be called immediately after start of " +
              "request/response headers/started."
      end
      if mode == :request
        request_body(data: d)
      else
        response_boyd(data: d)
      end
    end

    def request_body(options = {})
      event(REQUEST_BODY, options)
    end

    def request_finished(options = {})
      event(REQUEST_FINISHED, options)
    end

    def request(options)
      h = options.delete(:headers)
      d = options.delete(:body)
      request_started(options)
      headers(h) if h
      body(d) if d
      request_finished
    end

    def response_started(options = {})
      event(RESPONSE_STARTED, options)
    end

    def response_header(options = {})
      headers = options.delete(:headers)
      options[:header] ||= []
      if headers
        options[:header].concat(headers.collect do |k, v|
          {name: k, value: v}
        end)
      end
      event(RESPONSE_HEADER, options)
    end

    def response_header_finished(options = {})
      event(RESPONSE_HEADER_FINISHED, options)
    end

    def response_body(options = {})
      event(RESPONSE_BODY, options)
    end

    def response_finished(options = {})
      event(RESPONSE_FINISHED, options)
    end

    def response(options)
      h = options.delete(:headers)
      d = options.delete(:body)
      response_started(options)
      headers(h) if h
      body(d) if d
      response_finished
    end

    def connection_data_in(options = {})
      event(CONNECTION_DATA_IN, options)
    end

    def connection_data_out(options = {})
      event(CONNECTION_DATA_OUT, options)
    end

    def event(which, options={})
      events << make_event(which, options)
    end

  private
    attr_reader :events
  end
end

if $0 == __FILE__
  $:.unshift(File.dirname(__FILE__))
  require 'pp'
  require 'hash_to_pb'
  require 'json'

  if ARGV.empty? || ARGV.length > 2
    puts "Usage: #{$0} [option] <file>"
    exit 1
  end

  output_type = 'pb'
  if ARGV.length == 2
    case op = ARGV.shift
    when '--pb'   then output_type = 'pb'
    when '--json' then output_type = 'json'
    when '--ruby' then output_type = 'ruby'
    else
      puts "Unknown option: #{op}"
    end
  end

  text = IO::read(ARGV[0])

  case output_type
  when 'pb' then
    ClippScript::eval(text) do |input|
      IronBee::CLIPP::HashToPB::write_hash_to_pb(STDOUT, input)
    end
  when 'json' then
    inputs = ClippScript::eval(text)
    puts JSON.pretty_generate(inputs)
  when 'ruby' then
    inputs = ClippScript::eval(text)
    pp inputs
  else
    puts "Insanity error."
    exit 1
  end
end