module EapolTestHelper
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
end
