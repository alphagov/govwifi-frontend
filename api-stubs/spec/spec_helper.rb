ENV["RACK_ENV"] = "test"

require "rack/test"
require "rspec"
require File.expand_path "../app.rb", __dir__

module RSpecMixin
    include Rack::Test::Methods
    def app
        described_class
    end
end

RSpec.configure do |c|
    c.include RSpecMixin
    c.before(:each) do
      DB_AUTH[:lines].truncate
      DB_LOGGING[:lines].truncate
    end
end
