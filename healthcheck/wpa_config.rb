# frozen_string_literal: true

require "erb"

class WpaConfig
  def initialize(template_path, ssid:, identity:, password:)
    @template_path = template_path
    generate(ssid, identity, password)
  end

  def path
    # hack, remove .erb
    @template_path[0..-5]
  end

private

  def generate(ssid, identity, password)
    erb = ERB.new(File.read(@template_path))
    erb.filename = @template_path

    File.open(path, "w+") do |f|
      f.write(erb.result_with_hash(
                ssid: ssid,
                identity: identity,
                password: password,
              ))
    end
  end
end
