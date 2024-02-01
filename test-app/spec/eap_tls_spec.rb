require "sinatra/base"
require "json"
require 'sequel'
require 'sqlite3'
require "_spec_helper"

RSpec.describe 'EAP-TLS' do
  let(:eapol_test) { GovwifiEapoltest.new(radius_ips: ["127.0.0.1"], secret: "testing123") }
  let(:server_cert_path) { "/etc/raddb/certs/ca.pem" }
  it "accepts authentication with a valid certificate" do
    expect(eapol_test.run_eap_tls(client_cert_path: "/certificates/client.pem",
                                  client_key_path: "/certificates/client.key",
                                  server_cert_path:)).to all have_been_successful
  end

  it "rejects authentication with an invalid key" do
    expect(eapol_test.run_eap_tls(client_cert_path: "/certificates/client.pem",
                                  client_key_path: "/certificates/root_ca.key",
                                  server_cert_path:)).to all have_failed
  end

  it "rejects authentication with a chained certificate whose intermediate is not in the trusted certificate directory" do
    expect(eapol_test.run_eap_tls(client_cert_path: "/certificates/alt_combined_client.pem",
                                  client_key_path: "/certificates/alt_client.key",
                                  server_cert_path:)).to all have_failed
  end

  it "accepts authentication with a valid chained certificate" do
    expect(eapol_test.run_eap_tls(client_cert_path: "/certificates/combined_client.pem",
                                  client_key_path: "/certificates/client.key",
                                  server_cert_path:)).to all have_been_successful
  end
end
