#!/bin/bash

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR

git stash >> /dev/null 2>&1
stashRet=`echo $?`
git co master >> /dev/null 2>&1
rm -rf __tmp
mkdir -p __tmp/docs
rm -rf __tmp/docs/*

for doc in `ls docs/*`; do
    markdown $doc > __tmp/${doc%.md}.html
done

git checkout gh-pages >> /dev/null 2>&1
if [ "$stashRet" == "0" ]; then
    git stash pop >> /dev/null 2>&1
fi

HEADER='
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" type="text/css" href="../stylesheets/normalize.css" media="screen">
    <link href="https://fonts.googleapis.com/css?family=Open+Sans:400,700" rel="stylesheet" type="text/css">
    <link rel="stylesheet" type="text/css" href="../stylesheets/stylesheet.css" media="screen">
    <link rel="stylesheet" type="text/css" href="../stylesheets/github-light.css" media="screen">
  </head>
  <body>
    <section class="page-header">
      <h1 class="project-name">Zcm</h1>
      <h2 class="project-tagline">Zero Communications and Marshalling</h2>
      <a href="https://github.com/ZeroCM/zcm" class="btn">View on GitHub</a>
      <a href="http://ci.zcm-project.org" class="btn">Monitor Builds</a>
      <a href="https://gitter.im/ZeroCM/zcm" class="btn">Chat with the team</a>
      <a href="https://github.com/ZeroCM/zcm/zipball/master" class="btn">Download .zip</a>
      <a href="https://github.com/ZeroCM/zcm/tarball/master" class="btn">Download .tar.gz</a>
    </section>

    <section class="main-content">'
FOOTER='
    </section>
  </body>
</html>'

mkdir -p __tmp/newDocs

for doc in `ls __tmp/docs/`; do
    newDoc="__tmp/newDocs/$doc"
    echo "$HEADER" > $newDoc && \
    cat __tmp/docs/$doc >> $newDoc && \
    cat javascripts/googleAnalytics.js >> $newDoc && \
    echo "$FOOTER" >> $newDoc && \
    sed -i "s:\([a-zA-Z]*\)\.md:\1.html:g" $newDoc && \
    sed -i "s:README\.html:index.html:g" $newDoc
done

cp __tmp/newDocs/* docs/
git add --all docs/*
rm -rf __tmp
echo "Updated!"
