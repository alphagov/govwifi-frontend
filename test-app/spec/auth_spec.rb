require "sinatra/base"
require "json"
require 'sequel'
require 'sqlite3'
require "govwifi_eapoltest"
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
  PAP_CMD = "radtest testing password localhost 0 testing123"
  CHAP_CMD = "radtest -t chap testing password localhost 0 testing123"
  MSCHAP_CMD = "radtest -t mschap testing password localhost 0 testing123"

  it_behaves_like "it rejects authentication attempt", PAP_CMD
  it_behaves_like "it rejects authentication attempt", CHAP_CMD
  it_behaves_like "it rejects authentication attempt", MSCHAP_CMD

  let(:eapol_test) { GovwifiEapoltest.new(radius_ips: ["127.0.0.1"], secret: "testing123") }
  let(:username) { ENV.fetch("HEALTH_CHECK_IDENTITY") }
  let(:password) { ENV.fetch("HEALTH_CHECK_PASSWORD") }

  it "rejects authentication with the wrong password" do
    expect(eapol_test.run_peap_mschapv2(username:, password: "wrong_password")
    ).to all have_failed
  end

  it "rejects authentication with the wrong username" do
    expect(eapol_test.run_peap_mschapv2(username: "wrong", password:)
    ).to all have_failed
  end

  it "authenticates successfully with the correct username and password" do
    expect(eapol_test.run_peap_mschapv2(username:, password:)
    ).to all have_been_successful
  end

  it "logs a successful authentication attempt" do
    expect {
      eapol_test.run_peap_mschapv2(username:, password:)
    }.to change { LoggingLine.all.count }.by(1)
  end

end
