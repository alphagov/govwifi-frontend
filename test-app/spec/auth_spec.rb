require "sinatra/base"
require "json"
require 'sequel'
require 'sqlite3'
require "commands"
require "eapol_test_helper"
require "_spec_helper"

RSpec.shared_examples "it rejects authentication attempt" do |command|
  let(:command) { command }
  it "does not reach the authentication api or the logging api and rejects access" do
    output = `#{command}`
    expect(LoggingLine.all).to be_empty
    expect(AuthLine.all).to be_empty
    expect(output).to include("Access-Reject")
  end
end

RSpec.describe 'test' do
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
