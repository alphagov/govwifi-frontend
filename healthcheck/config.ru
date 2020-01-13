# frozen_string_literal: true

require "./app"

RACK_ENV = ENV["RACK_ENV"] ||= "development"

run App
