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
          docker.build("${env.IMAGE_NAME}:staging")
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
          withDockerRegistry(credentialsId: 'ecr:eu-west-2:jenkins-deploy', url: env.AWS_ECS_API_REGISTRY) {
            docker.image("${env.IMAGE_NAME}:staging").push()
          }
        }
      }
    }
  }
}
