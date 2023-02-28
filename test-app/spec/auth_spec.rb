require "sinatra/base"
require "json"
require 'sequel'
require 'sqlite3'

require "commands"

DB_AUTH = Sequel.connect(adapter: 'sqlite', database: ENV.fetch("AUTH_DB"))
DB_LOGGING = Sequel.connect(adapter: 'sqlite', database: ENV.fetch("LOGGING_DB"))

class AuthLine < Sequel::Model(DB_AUTH[:lines])
end

class LoggingLine < Sequel::Model(DB_LOGGING[:lines])
end

RSpec.shared_examples "it rejects authentication attempt" do |command|
  let(:command) { command }
  it "does not reach the authentication api or the logging api and rejects access" do
    output = `#{command}`
    expect(LoggingLine.all).to be_empty
    expect(AuthLine.all).to be_empty
    expect(output).to include("Access-Reject")
  end
end

def run_eapol(config_template_path, variables={})
  variables_with_defaults = { phase1: "",
                              phase2: "",
                              username: ENV.fetch("HEALTH_CHECK_IDENTITY"),
                              password: ENV.fetch("HEALTH_CHECK_PASSWORD") }.merge(variables)
  file = Tempfile.new
  file.write ERB.new(File.read(config_template_path)).result_with_hash(variables_with_defaults)
  file.close

  command = "eapol_test -a 127.0.0.1 -c #{file.path} -s testing123"
  `#{command}`
ensure
  file.unlink
end

RSpec.describe 'test' do
  before :each do
    LoggingLine.truncate
    AuthLine.truncate
  end

  it_behaves_like "it rejects authentication attempt", PAP_CMD
  it_behaves_like "it rejects authentication attempt", CHAP_CMD
  it_behaves_like "it rejects authentication attempt", MSCHAP_CMD

  it "rejects authentication with the wrong password" do
    output = run_eapol(PEAP_MSCHAPv2_CONFIG_PATH,
                       username: ENV.fetch("HEALTH_CHECK_IDENTITY"),
                       password: "wrong_password")
    expect(output).to include("FAILURE")
  end

  it "rejects authentication with the wrong username" do
    output = run_eapol(PEAP_MSCHAPv2_CONFIG_PATH,
                       username: "wrong_username",
                       password: ENV.fetch("HEALTH_CHECK_PASSWORD"))
    expect(output).to include("FAILURE")
  end

  it "authenticates successfully with the correct username and password" do
    output = run_eapol(PEAP_MSCHAPv2_CONFIG_PATH,
                       username: ENV.fetch("HEALTH_CHECK_IDENTITY"),
                       password: ENV.fetch("HEALTH_CHECK_PASSWORD"))
    expect(output).to include("SUCCESS")
  end

  it "logs a successful authentication attempt" do
    expect {
      run_eapol(PEAP_MSCHAPv2_CONFIG_PATH)
    }.to change { LoggingLine.all.count }.by(1)
  end

end
