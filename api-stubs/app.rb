require "sinatra/base"
require "json"
require 'sequel'
require 'sqlite3'

DB_AUTH = Sequel.connect(adapter: 'sqlite', database: ENV.fetch("AUTH_DB"))
DB_LOGGING = Sequel.connect(adapter: 'sqlite', database: ENV.fetch("LOGGING_DB"))

class AuthLine < Sequel::Model(DB_AUTH[:lines])
end

class LoggingLine < Sequel::Model(DB_LOGGING[:lines])
end

class ApiStub < Sinatra::Base
  configure do
    set :port, 80
  end

  get "/authorize/user/:name/*" do
    if params["name"] == ENV["HEALTH_CHECK_IDENTITY"]
      line = AuthLine.create(line: request.path_info)
      puts "** #{line.to_hash}"
      content_type :json
      { "control:Cleartext-Password": ENV["HEALTH_CHECK_PASSWORD"] }.to_json
    else
      status 404
    end
  end

  post "/logging/post-auth" do
    request.body.rewind
    line = LoggingLine.create(JSON.parse(request.body.read))
    puts "** #{line.to_hash}"
    content_type :json
    status 204
  end

  get "/healthcheck" do
    status 200
  end

end
