pipeline {
  agent none

  options {
    ansiColor('xterm')
  }

  environment {
    IMAGE_NAME = 'govwifi/frontend'
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

    stage('Build deployment image') {
      agent any
      when {
        branch 'master'
        beforeAgent true
      }

      steps {
        script {
          docker.build("${env.IMAGE_NAME}:testing")
        }
      }
    }

    stage('Publish Staging Image') {
      agent any
      when {
        branch 'master'
        beforeAgent true
      }

      steps {
        script {
          docker.withRegistry(env.AWS_ECS_API_REGISTRY, 'ecr:eu-west-2:jenkins-deploy') {
            docker.image("${env.IMAGE_NAME}:testing").push()
          }
        }
      }
    }
  }
}
