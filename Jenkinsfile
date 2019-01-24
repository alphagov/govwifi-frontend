pipeline {
  agent none
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
