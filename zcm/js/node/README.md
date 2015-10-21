Bindings to Zero Communications and Marshalling
===============================================

If you are on a Debian system, you MUST install the `nodejs-legacy` package as well as the `nodejs`
and `npm` packages. The `nodejs-legacy` package is required because Debian decided to change the
name of the Node.js executable from `node` to `nodejs` because of some package naming collision.
This causes lots of problems with some packages (namely the native code compiler `node-gyp`) and
will cause the normal installation of our dependencies to break. By installing the `nodejs-legacy`
package, a symlink will be created so that `node` will call the normal `nodejs` binary.
