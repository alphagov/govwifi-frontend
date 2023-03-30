DB_LOGGING = Sequel.connect(adapter: 'sqlite', database: ENV.fetch("LOGGING_DB"))

class LoggingLine < Sequel::Model(DB_LOGGING[:lines])
end
