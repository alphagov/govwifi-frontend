require "sinatra/base"
require "json"
require 'sequel'
require 'sqlite3'
require "commands"
require "_spec_helper"

RSpec.describe 'test' do
  it "accepts authentication with a valid certificate" do
    output = run_eapol(EAP_TLS_CONFIG_PATH,
                       client_cert_path: "/certificates/client.pem",
                       client_key_path: "/certificates/client.key",
                       server_cert_path: "/etc/raddb/certs/ca.pem")
    expect(output).to include("SUCCESS")
  end

  it "rejects authentication with an invalid key" do
    output = run_eapol(EAP_TLS_CONFIG_PATH,
                       client_cert_path: "/certificates/client.pem",
                       client_key_path: "/certificates/root_ca.key",
                       server_cert_path: "/etc/raddb/certs/ca.pem")
    expect(output).to include("FAILURE")
  end


  it "rejects authentication with a chained certificate whose intermediate is not in the trusted certificate directory" do
    output = run_eapol(EAP_TLS_CONFIG_PATH,
                       client_cert_path: "/certificates/alt_combined_client.pem",
                       client_key_path: "/certificates/alt_client.key",
                       server_cert_path: "/etc/raddb/certs/ca.pem")
    expect(output).to include("FAILURE")
  end

  it "accepts authentication with a valid chained certificate" do
    output = run_eapol(EAP_TLS_CONFIG_PATH,
                       client_cert_path: "/certificates/combined_client.pem",
                       client_key_path: "/certificates/client.key",
                       server_cert_path: "/etc/raddb/certs/ca.pem")
    expect(output).to include("SUCCESS")
  end
end
