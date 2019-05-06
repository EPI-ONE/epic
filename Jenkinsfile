#!groovy

def labels = ['bionic', 'xenial'] // labels for Jenkins node types we will build on
def builders = [:]
for (x in labels) {
    def label = x // Need to bind the label variable before the closure - can't do 'for (label in labels)'

    // Create a map to pass in to the 'parallel' step so we can fire all the builds at once
    builders[label] = {
      node(label) {
        try {
          def BUILD_DIR = "build"
          def COVERAGE_DIR = "coverage"
          stage('build') {
            checkout([$class: 'GitSCM', branches: [[name: '*/master']], doGenerateSubmoduleConfigurations: false, extensions: [[$class: 'CleanBeforeCheckout'], [$class: 'CleanCheckout'], [$class: 'LocalBranch']], submoduleCfg: [], userRemoteConfigs: [[url: 'git@github.com:reijz/epic.git']]])
            sh "rm -rf ${BUILD_DIR}"
            sh "mkdir ${BUILD_DIR}"
            dir("${BUILD_DIR}") {
              sh 'cmake ..'
              sh 'make -j2'
            }
          }

          stage('unittest') {
            timeout(10) {
              dir("bin") {
                sh './epictest --gtest_output=xml:gtestall.xml'
              }
            }
            xunit([GoogleTest(deleteOutputFiles: true, failIfNotNew: true, pattern: 'bin/gtestall.xml', skipNoTestFiles: false, stopProcessingIfError: true)])
          }

          stage('coverage') {
            if (env.NODE_NAME == 'master') {
              sh "rm -rf ${COVERAGE_DIR}"
              sh "mkdir ${COVERAGE_DIR}"
              sh "lcov --directory ${BUILD_DIR}/CMakeFiles/epictest.dir/ --gcov-tool llvm-gcov.sh --capture -o ${COVERAGE_DIR}/cov.info"
              sh "genhtml ${COVERAGE_DIR}/cov.info -o ${COVERAGE_DIR}"
              publishHTML([allowMissing: false, alwaysLinkToLastBuild: false, keepAll: false, reportDir: 'coverage', reportFiles: 'index.html', reportName: 'coverage', reportTitles: 'coverage'])
            }
          }
          currentBuild.result = 'SUCCESS'
        } catch (Exception err) {
          currentBuild.result = 'FAILURE'
        } finally {
          def branch = sh(returnStdout: true, script: "git symbolic-ref --short -q HEAD")
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
