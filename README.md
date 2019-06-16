Experimental surrogate decomposition
====================================

This is a prototype implementation of the mathematical optimisation
approach described in
[No regret surrogate decomposition](https://www.authorea.com/users/2541/articles/324048-no-regret-surrogate-decomposition?access_token=7PRDD1XaWLjc8wglr1_PLg).
The decomposition approach re-purposes online learning algorithms,
like the usual [reduction of linear programming to online learning](https://pvk.ca/Blog/2019/04/23/fractional-set-covering-with-experts/),
but attempts to preserve more structure and thus obtain stronger
bounds while exposing more parallelisation opportunity.

The goals are:

1. Figure out the correct high-level structure to make this efficient,
   on multicore CPUs, GPUs, and perhaps in distributed solvers.
2. Explore low-level numerical efficiency issues in
   [AdaHedge](https://arxiv.org/abs/1301.0534) and
   [NormalHedge](https://arxiv.org/abs/1502.05934)
3. Figure out how to solve linear knapsacks quickly on contemporary
   architectures.

Quick start
-----------

This project uses [Bazel](https://bazel.build/) to manage dependencies
and build executables.  It is known to work with Bazel 0.25.2 and
0.26.1.  If you would like to avoid installing Bazel, you can also
substitute `./bazelisk` for all `bazel` invocation below:
[Bazelisk](https://github.com/bazelbuild/bazelisk) will download a
stable version of Bazel, cache it in "$HOME/.cache/bazelisk", and
forward all arguments to the cached Bazel.

`bazel run -c opt :random-set-cover` will build and execute program
that generates a random set cover instance, and solves it with the
decomposition approach.  `bazel run -c opt :random-set-cover --
-helpfull` will list the command line flags for the executable.
`-max_set_per_value` controls density and `-num_sets` / `-num_values`
control the size of the instance.

When running on a machine that has libglfw3 and its development
headers, `bazel run --define gui=yes -c opt :visualizer` generates and
solves random instances exactly like `:random-set-cover`, but also
displays statistics about the solution process in a GUI.

Development
-----------

`bazel test :all` runs all unit tests.  `./sanitize.sh` shows how to
do the same under
[`ASan`](https://clang.llvm.org/docs/AddressSanitizer.html) and
[`UBSan`](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html).

`./coverage.sh` generates a coverage report for the same unit tests;
the report goes in `coverage/index.html`.

Before committing
-----------------

Always run all unit tests, and consider doing do with sanitizers.

This project uses the Google C++ style as enforced by
[`clang-format`](https://clang.llvm.org/docs/ClangFormat.html).
`./reformat.sh` shows how to apply that style to all the source files
in the repository.

Bazel builds (BUILD, WORKSPACE, etc.) should be checked with
[`buildifier`](https://github.com/bazelbuild/buildtools).  To reformat
and check all files: `bazel run -c opt :buildifier -- -lint=fix
"$PWD/"{BUILD,WORKSPACE,BUILD.gl3w,BUILD.imgui}`.

Tooling
-------

Some IDEs, like [Visual Studio Code](https://code.visualstudio.com/)
(along with its C++ plugin, `ms-vscode.cpptools`) automatically index
the project given its bazel WORKSPACE.  If you're using VS Code, the
Bazel plugin `bazelbuild.vscode-bazel` may also be of interest.

Less clever system may need a compilation database.
`generate-compile-commmands.sh` will generate one in the default
location, `compile_commands.json` at the toplevel of the project.  Any
clang-based tooling can be pointed at it, e.g.,
[YouCompleteMe](https://github.com/Valloric/YouCompleteMe) or
[ccls](https://github.com/MaskRay/emacs-ccls).

Code Indexing
-------------

I find most of the value in development tooling is actually code
exploration, and dedicated programs may then be more appropriate, if
only because they help us stay out of the "actively coding" mindset.
[Kythe](https://kythe.io/)'s demo UI isn't [Code
Search](https://cs.chromium.org/), but it's better than nothing.

If you have docker installed, `kythe/index.sh` will (slowly, get
coffee once it's going) generate a servable code search index.  Once
the index has been generated, `kythe/serve.sh $PORT` will serve it on
localhost.

The thing most likely to go wrong during indexing is a network IO
error when `kythe/index.sh` runs `bazel build` internally.
Re-executing `kythe/index.sh` a minute later usually fixes it (later
invocation will be able to use the cache in `kythe_out`).

For everything else, there's `kythe/cleanup.sh`, which deletes
everything in `kythe_out` and should yield a clean slate.
