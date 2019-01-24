pipeline {
  agent none

  options {
    ansiColor('xterm')
  }

  stages {
    stage('Linting') {
      agent any
      steps {
        sh 'make lint'
      }
    }

    stage('Test') {
      agent any
      steps {
        sh 'make test'
      }
    }
  }
}
