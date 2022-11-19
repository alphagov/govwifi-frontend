# frozen_string_literal: true

require "sinatra/base"
require "logger"

require_relative "./wpa_config"

class App < Sinatra::Base
  configure do
    set :port, 3000
    set :wpa_config_template, File.join(root, "peap-mschapv2.conf.erb")
    set(
      :wpa_config,
      WpaConfig.new(
        settings.wpa_config_template,
        ssid: ENV["HEALTH_CHECK_SSID"],
        identity: ENV["HEALTH_CHECK_IDENTITY"],
        password: ENV["HEALTH_CHECK_PASSWORD"],
      ),
    )
    set :health_check_key, ENV["HEALTH_CHECK_RADIUS_KEY"]
  end

  configure :production, :staging, :development do
    enable :logging
  end

  get "/" do
    eapol_test(settings.wpa_config.path)
  end

  post "/" do
    wpa_config = WpaConfig.new(
      settings.wpa_config_template,
      ssid: ENV["HEALTH_CHECK_SSID"],
      identity: params[:identity],
      password: params[:password],
    )
    result = eapol_test(wpa_config.path)
    wpa_config.close

    result
  end

private

  def eapol_test(path)
    health_check_key = settings.health_check_key
    result = `eapol_test -r0 -t3 -c #{path} -a 127.0.0.1 -s #{health_check_key}`
    last_result = result.split("\n").last
    last_result == "SUCCESS" ? 200 : 500
  end
end
