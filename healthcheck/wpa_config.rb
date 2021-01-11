# frozen_string_literal: true

require "erb"

class WpaConfig
  def initialize(template_path, ssid:, identity:, password:)
    @template_path = template_path
    @file = Tempfile.new(template_path)
    generate(ssid, identity, password)
  end

  def path
    @file.path
  end

  def close
    @file.close
    @file.unlink
  end

private

  def generate(ssid, identity, password)
    erb = ERB.new(File.read(@template_path))
    erb.filename = @template_path

    @file.write(erb.result_with_hash(
      ssid: ssid,
      identity: identity,
      password: password,
    ))
    @file.rewind
  end
end
