class Globals {
  static boolean userInput = true
  static boolean didTimeout = false
}

pipeline {
  agent none

  options {
    ansiColor('xterm')
  }

  environment {
    IMAGE_NAME = 'govwifi/frontend'
    ECR_CREDENTIAL = 'ecr:eu-west-2:jenkins-deploy'
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

    stage('Confirm deploy to staging') {
      agent none
      when {
        branch 'master'
        beforeAgent true
      }
      steps {
        wait_for_input('staging')
      }
    }

    stage('Publish Staging Image') {
      agent any
      when {
        branch 'master'
        beforeAgent true
      }

      steps {
        deploy('testing-latest')
      }
    }

    stage('Confirm deploy to production') {
      agent none
      when {
        branch 'master'
        beforeAgent true
      }
      steps {
        wait_for_input('production')
      }
    }

    stage('Publish Production Image') {
      agent any
      when {
        branch 'master'
        beforeAgent true
      }

      steps {
        deploy('testing-latest')
      }
    }
  }
}

def wait_for_input(deploy_environment) {
  if (deployCancelled()) {
    return;
  }
  try {
    timeout(time: 5, unit: 'MINUTES') {
      input "Do you want to deploy to ${deploy_environment}?"
    }
  } catch (err) {
    def user = err.getCauses()[0].getUser()

    if('SYSTEM' == user.toString()) { // SYSTEM means timeout.
      Globals.didTimeout = true
      echo "Release window timed out, to deploy please re-run"
    } else {
      Globals.userInput = false
      echo "Aborted by: [${user}]"
    }
  }
}

def deploy(image_tag) {
  if (deployCancelled()) {
    return;
  }

  docker.withRegistry(env.AWS_ECS_API_REGISTRY, env.ECR_CREDENTIAL) {
    docker.build("${env.IMAGE_NAME}:${image_tag}").push()
  }
}

def deployCancelled() {
  return (Globals.didTimeout || !Globals.userInput);
}
