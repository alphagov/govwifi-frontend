require 'spec_helper'

RSpec.describe ApiStub do
  describe "stubs" do
    describe "/authorize/user/:name" do
      describe "wrong username" do
        let(:url) { "/authorize/user/wrong/abc/def" }
        it "returns 404" do
          get url
          expect(last_response).to_not be_ok
        end
        it "does not adds one log line" do
          expect {
            get url
          }.to_not change(DB_AUTH[:lines], :count)
        end
      end
      describe "correct username" do
        let(:url) { "/authorize/user/#{ENV["HEALTH_CHECK_IDENTITY"]}/abc/def" }
        it "returns ok" do
          get url
          expect(last_response).to be_ok
        end
        it "logs the url" do
          get url
          expect(DB_AUTH[:lines].find(line: url)).to_not be_nil
        end
        it "adds one log line" do
          expect {
            get url
          }.to change(DB_AUTH[:lines], :count).by(1)
        end
        it "returns the password" do
          get url
          expect(last_response.body).to eq({ "control:Cleartext-Password": ENV["HEALTH_CHECK_PASSWORD"] }.to_json)
        end
      end
    end

    describe "/logging/post-auth" do
      let(:values) {
        { username: "john", mac: "aaa", called_station_id: "bbb", site_ip_address: "ccc",
          cert_name: "ddd", authentication_result: "eee", "Access-Accept" => "fff",
          authentication_reply: "ggg", task_id: "hhh", cert_serial: "iii", cert_subject: "jjj", cert_issuer: "kkk" }
      }
      let(:body) {
        values.to_json
      }
      it "returns 204" do
        post "/logging/post-auth", body
        expect(last_response.status).to be 204
      end
      it "logs the body" do
        post "/logging/post-auth", body
        expect(DB_LOGGING[:lines].find(values)).to_not be nil
      end
      it "adds a log line" do
        expect {
          post "/logging/post-auth", body
        }.to change(DB_LOGGING[:lines], :count).by(1)
      end
    end

    describe "/healthcheck" do
      it "returns 200" do
        get "/healthcheck"
        expect(last_response).to be_successful
      end
    end
  end
end
