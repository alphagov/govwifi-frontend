ET_CONFIGS_PATH = File.expand_path('spec/fixtures')

EAP_TTLS_CONFIG_PATH = File.join(ET_CONFIGS_PATH, "eap-ttls.conf.erb")

PEAP_MSCHAPv2_CONFIG_PATH = File.join(ET_CONFIGS_PATH, "peap-mschapv2.conf.erb")

EAP_TLS_CONFIG_PATH = File.join(ET_CONFIGS_PATH, "eap-tls.conf.erb")

SERVICE_STATUS = "radclient -x localhost:18121 status testing123"

PAP_CMD = "radtest testing password localhost 0 testing123"

CHAP_CMD = "radtest -t chap testing password localhost 0 testing123"

MSCHAP_CMD = "radtest -t mschap testing password localhost 0 testing123"
