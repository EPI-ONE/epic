#!groovy

def labels = ['bionic', 'xenial'] // labels for Jenkins node types we will build on
def builders = [:]
for (x in labels) {
    def label = x // Need to bind the label variable before the closure - can't do 'for (label in labels)'

    // Create a map to pass in to the 'parallel' step so we can fire all the builds at once
    builders[label] = {
      node(label) {
        try {
          stage('build') {
            checkout scm
            sh 'rm -rf build'
            sh 'mkdir build'
            dir("build") {
              sh 'cmake ..'
              sh 'make -j2'
            }
          }

          stage('unittest') {
            dir("bin") {
              sh './epictest --gtest_output=xml:gtestall.xml'
            }

            xunit([GoogleTest(deleteOutputFiles: true, failIfNotNew: true, pattern: 'bin/gtestall.xml', skipNoTestFiles: false, stopProcessingIfError: true)])
          }
          currentBuild.result = 'SUCCESS'
        } catch (Exception err) {
          currentBuild.result = 'FAILURE'
        } finally {
          def branch = sh(returnStdout: true, script: "git symbolic-ref --short -q HEAD")
          def slack_color
          if (currentBuild.currentResult == 'SUCCESS') {
            color = "good"
          } else {
            color = "danger"
          }
          slackSend(color: "${color}", message: "Build ${currentBuild.currentResult} : ${currentBuild.fullDisplayName} branch ${branch} on ${env.NODE_LABELS}. Detail: ${currentBuild.absoluteUrl}")
        }
      }
    }
}

parallel builders
