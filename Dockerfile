FROM ubuntu:16.04
USER root

ENV ZCM_HOME /zcm

RUN apt-get update && apt-get install -y sudo apt-utils

COPY scripts/ $ZCM_HOME/scripts

RUN bash -c '$ZCM_HOME/scripts/install-deps.sh -i -s'

COPY config/ $ZCM_HOME/config
COPY docs/ $ZCM_HOME/docs
COPY examples/ $ZCM_HOME/examples
COPY gen/ $ZCM_HOME/gen
COPY test/ $ZCM_HOME/test
COPY tools/ $ZCM_HOME/tools
COPY util/ $ZCM_HOME/util
COPY waf $ZCM_HOME/waf
COPY waftools/ $ZCM_HOME/waftools
COPY wscript $ZCM_HOME/wscript
COPY zcm/ $ZCM_HOME/zcm

RUN bash -c 'export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::") && \
             export NVM_DIR=$HOME/.nvm && \
             [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" && \
             export PATH=$PATH:$ZCM_HOME/deps/julia/bin && \
             cd $ZCM_HOME && \
             ./waf distclean configure --use-all --use-dev && \
             ./waf build && \
             ./waf install && \
             ./waf build_examples'
