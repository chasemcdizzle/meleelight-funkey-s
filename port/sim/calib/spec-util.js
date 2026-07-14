// spec-util.js — M2 task 1 capture spec: the util/math substrate module
// boundaries (injected AFTER capturelib.js). Registers window.__capSpecs.util.
//
// Boundary (34 wrapped functions across 11 modules):
// - Vec2D module: getXOrYCoord, putXOrYCoord, flipXOrY + the prototype
//   method Vec2D#dot (records [thisState, arg]). The Vec2D CONSTRUCTOR is
//   deliberately NOT wrapped: it is a two-field record initializer with no
//   computation, exercised bit-exactly by every Vec2D value flowing
//   through every other captured boundary (documented judgment, fix_plan
//   §M2 conventions).
// - linAlg: all 11 exports.
// - solveQuadraticEquation, lineAngle, extremePoint, zipLabels, toList: 1 each.
// - findSmallestWithin: both exports.
// - ecbTransform: all 5 exports.
// - detectIntersections: all 4 exports.
// - Segment2D: constructor (Segment2D.new) + per-instance closure methods
//   segLength/project (Segment2D#segLength, Segment2D#project, recorded
//   with [thisState, ...args]).
(() => {
  window.__capSpecs.util = {
    expectWrapped: 34,
    install(ctx) {
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(ctx.cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      // --- Vec2D module ----------------------------------------------------
      const vec2d = find((ex) =>
        typeof ex.Vec2D === "function" &&
        typeof ex.getXOrYCoord === "function" &&
        typeof ex.putXOrYCoord === "function" &&
        typeof ex.flipXOrY === "function", "Vec2D");
      ctx.wrapExport(vec2d, "getXOrYCoord");
      ctx.wrapExport(vec2d, "putXOrYCoord");
      ctx.wrapExport(vec2d, "flipXOrY");
      // prototype method — dispatched through the prototype at call time,
      // so module-internal calls are captured too (extra coverage, still
      // value-faithful).
      {
        const origDot = vec2d.Vec2D.prototype.dot;
        if (typeof origDot !== "function") throw new Error("capture: Vec2D.prototype.dot missing");
        vec2d.Vec2D.prototype.dot = function (vector) {
          const argsCanon = "[" + ctx.canon(this) + "," + ctx.canon(vector) + "]";
          const ret = origDot.apply(this, arguments);
          ctx.push("Vec2D#dot", argsCanon, ctx.canon(ret));
          return ret;
        };
        ctx.declare("Vec2D#dot");
        ctx.wrapped++;
      }

      // --- linAlg ------------------------------------------------------------
      const linAlg = find((ex) =>
        typeof ex.dotProd === "function" &&
        typeof ex.inverseMatrix === "function" &&
        typeof ex.reflect === "function", "linAlg");
      for (const n of ["dotProd", "scalarProd", "norm", "add", "subtract",
                       "euclideanDist", "manhattanDist", "orthogonalProjection",
                       "inverseMatrix", "multMatVect", "reflect"]) {
        ctx.wrapExport(linAlg, n);
      }

      // --- small pure modules -------------------------------------------------
      const solveQ = find((ex) => typeof ex.solveQuadraticEquation === "function",
                          "solveQuadraticEquation");
      ctx.wrapExport(solveQ, "solveQuadraticEquation");

      const lineAngleM = find((ex) => typeof ex.lineAngle === "function", "lineAngle");
      ctx.wrapExport(lineAngleM, "lineAngle");

      const fsw = find((ex) =>
        typeof ex.findSmallestWithin === "function" &&
        typeof ex.pickSmallestSweep === "function", "findSmallestWithin");
      ctx.wrapExport(fsw, "findSmallestWithin");
      ctx.wrapExport(fsw, "pickSmallestSweep");

      const extremeM = find((ex) => typeof ex.extremePoint === "function", "extremePoint");
      ctx.wrapExport(extremeM, "extremePoint");

      const ecbT = find((ex) =>
        typeof ex.moveECB === "function" &&
        typeof ex.squashECBAt === "function" &&
        typeof ex.makeECB === "function", "ecbTransform");
      for (const n of ["moveECB", "squashECBAt", "ecbFocusFromAngularParameter",
                       "interpolateECB", "makeECB"]) {
        ctx.wrapExport(ecbT, n);
      }

      const zipL = find((ex) => typeof ex.zipLabels === "function", "zipLabels");
      ctx.wrapExport(zipL, "zipLabels");

      const toL = find((ex) => typeof ex.toList === "function", "toList");
      ctx.wrapExport(toL, "toList");

      const dI = find((ex) =>
        typeof ex.distanceToPolygon === "function" &&
        typeof ex.intersectsAny === "function" &&
        typeof ex.lineDistanceToLines === "function", "detectIntersections");
      for (const n of ["intersectsAny", "distanceToLine", "distanceToPolygon",
                       "lineDistanceToLines"]) {
        ctx.wrapExport(dI, n);
      }

      // --- Segment2D (constructor + per-instance closure methods) -----------
      const segM = find((ex) => typeof ex.Segment2D === "function", "Segment2D");
      {
        const Orig = segM.Segment2D;
        function Segment2DWrapped(x, y, vecx, vecy) {
          Orig.apply(this, arguments);
          ctx.push("Segment2D.new",
                   ctx.canon(Array.prototype.slice.call(arguments)),
                   ctx.canon(this));
          const origSegLength = this.segLength;
          const origProject = this.project;
          this.segLength = function () {
            const argsCanon = "[" + ctx.canon(this) + "]";
            const ret = origSegLength.apply(this, arguments);
            ctx.push("Segment2D#segLength", argsCanon, ctx.canon(ret));
            return ret;
          };
          this.project = function (segOnto) {
            const argsCanon = "[" + ctx.canon(this) + "," + ctx.canon(segOnto) + "]";
            const ret = origProject.apply(this, arguments);
            ctx.push("Segment2D#project", argsCanon, ctx.canon(ret));
            return ret;
          };
        }
        Segment2DWrapped.prototype = Orig.prototype;
        segM.Segment2D = Segment2DWrapped;
        ctx.declare("Segment2D.new");
        ctx.declare("Segment2D#segLength");
        ctx.declare("Segment2D#project");
        ctx.wrapped += 3;
      }

      return { moduleIds: moduleIds };
    },
  };
})();
