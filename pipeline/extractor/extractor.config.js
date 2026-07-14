/* pipeline/extractor/extractor.config.js — webpack 1 config for the
 * extractor bundle, run INSIDE the upstream clone under docker node:8
 * (copied there by build-extractor.sh, modeled on
 * bin/webpack/animations.config.js + createConfig.js).
 *
 * Loader semantics mirror the game build's happypack babel query exactly
 * (presets es2015, plugins transform-flow-strip-types +
 * transform-class-properties — createConfig.js uses the SAME query for
 * dev and prod), so the extractor executes code transformed identically
 * to the shipped bundle. No eslint preloader, no happypack/notifier/
 * sw-precache (build-time conveniences, zero effect on emitted code),
 * no devtool (no eval wrappers).
 */

const path = require("path");
const webpack = require("webpack");
const srcPath = path.join(process.cwd(), "src");
const distJsPath = path.join(process.cwd(), "dist/js");

module.exports = {
  cache: false,
  debug: false,
  entry: [path.join(srcPath, "__extractor__.js")],
  output: {
    path: distJsPath,
    filename: "extractor.js",
  },
  resolve: {
    extensions: ["", ".js"],
    root: [srcPath],
  },
  module: {
    loaders: [
      {
        test: /\.jsx?$/,
        exclude: [/node_modules/],
        loader: "babel-loader",
        query: {
          presets: ["es2015"],
          plugins: ["transform-flow-strip-types", "transform-class-properties"],
        },
      },
    ],
  },
  // Stage modules (ystory/fountain) top-level-import engine modules but
  // reference them ONLY inside movingPlatforms/updatePlatform function
  // bodies, which extraction never calls (the data literals are verified
  // self-contained). Stub the EXACT request strings so main.js (the
  // god-module) and its DOM tentacles never enter the bundle;
  // build-extractor.sh hard-fails the build if "document." leaks in.
  externals: {
    "main/main": "var {}",
    "../../main/main": "var {}",
    "stages/activeStage": "var {}",
    "../activeStage": "var {}",
    "../../physics/environmentalCollision": "var {}",
  },
  plugins: [
    new webpack.DefinePlugin({
      "process.env": { NODE_ENV: '"dev"' },
    }),
  ],
};
