DB_AUTH = Sequel.connect(adapter: 'sqlite', database: ENV.fetch("AUTH_DB"))

class AuthLine < Sequel::Model(DB_AUTH[:lines])
end
