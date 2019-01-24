require 'sinatra/base'
require 'logger'

require_relative './wpa_config.rb'

class App < Sinatra::Base
  configure do
    set(:wpa_config, Proc.new {
      WpaConfig.new(
        File.join(root, 'peap-mschapv2.conf.erb'),
        ssid: ENV['HEALTH_CHECK_SSID'],
        identity: ENV['HEALTH_CHECK_IDENTITY'],
        password: ENV['HEALTH_CHECK_PASSWORD'],
      )
    })
    set :healt_check_key, ENV['HEALTH_CHECK_RADIUS_KEY']
  end

  configure :production, :staging, :development do
    enable :logging
  end

  get '/' do
    result = `eapol_test -r0 -t3 -c #{settings.wpa_config.path} -a 127.0.0.1 -s #{settings.healt_check_key}`
    last_result = result.split("\n").last
    last_result == 'SUCCESS' ? 200 : 500
  end
end
