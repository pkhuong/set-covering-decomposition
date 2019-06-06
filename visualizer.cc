#include <stdio.h>

#include <iostream>
#include <thread>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "external/gl3w/GL/gl3w.h"
#include "external/imgui/imgui.h"
#include "external/imgui/imgui_impl_glfw.h"
#include "external/imgui/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>  // NOFORMAT

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/types/span.h"
#include "random-set-cover-flags.h"
#include "random-set-cover-instance.h"
#include "set-cover-solver.h"
#include "solution-stats.h"

ABSL_FLAG(bool, dark_mode, true, "Enable dark mode theme");
ABSL_FLAG(size_t, history_limit, 100,
          "Show up to this many historical data points.");

namespace {
struct StateCache {
  std::vector<double> solution;
  SetCoverSolver::ScalarState scalar;

  double obj_value = 0;
  double max_violation = 0;
  std::vector<double> infeas;
  std::vector<float> infeas_bins;
  std::vector<float> solution_bins;
  std::vector<float> non_zero_solution_bins;

  std::vector<float> iteration_times;
  std::vector<float> prepare_times;
  std::vector<float> knapsack_times;
  std::vector<float> observe_times;
  std::vector<float> update_times;

  std::vector<float> sum_mix_gaps;
  std::vector<float> min_losses;
  std::vector<float> max_losses;

  std::vector<float> best_bounds;
  std::vector<float> delta_best_bounds;    // difference between iterations
  std::vector<float> best_bound_avg_gaps;  // best_bound - avg_solution_value.
  std::vector<float> avg_solution_values;
  std::vector<float> avg_solution_feasilities;
  std::vector<float> solution_values;
};

void glfw_error_callback(int error, const char* description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int setup(GLFWwindow** window_ptr) {
  *window_ptr = nullptr;
  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return 1;

// Decide GL+GLSL versions
#if __APPLE__
  // GL 3.2 + GLSL 150
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
// glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
// glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(
      1280, 720, "Surrogate decomposition visualizer", nullptr, nullptr);
  *window_ptr = window;
  if (window == nullptr) return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync

  // Initialize OpenGL loader
  if (gl3wInit() != 0) {
    fprintf(stderr, "Failed to initialize OpenGL loader!\n");
    return 1;
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  // Setup Dear ImGui style
  if (absl::GetFlag(FLAGS_dark_mode)) {
    ImGui::StyleColorsDark();
  } else {
    ImGui::StyleColorsClassic();
  }

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  return 0;
}

// Adds `observation` to the front of `out`, and truncates history if
// necessary.
void AddPoint(float observation, std::vector<float>* out) {
  const size_t limit = absl::GetFlag(FLAGS_history_limit);

  out->insert(out->begin(), observation);
  if (out->size() > limit) {
    out->resize(limit);
  }
}

// Adds `observation` in milliseconds to the front of out, and
// truncates history if necessary.
void AddTimeObservation(const absl::Duration observation,
                        std::vector<float>* out) {
  AddPoint(1000 * absl::ToDoubleSeconds(observation), out);
}

void UpdateDerivedValues(const RandomSetCoverInstance& instance,
                         StateCache* cache) {
  const double kFeasEps = absl::GetFlag(FLAGS_feas_eps);

  cache->obj_value =
      ComputeObjectiveValue(cache->solution, instance.obj_values);
  std::tie(cache->max_violation, cache->infeas) =
      ComputeCoverInfeasibility(cache->solution, instance.sets_per_value);
  {
    const auto infeas_bins = BinValues(cache->infeas, 100, kFeasEps);
    cache->infeas_bins.clear();
    for (const auto& entry : infeas_bins) {
      cache->infeas_bins.push_back(entry.second);
    }
  }

  {
    const auto sol_bins = BinValues(cache->solution, 100, kFeasEps);
    cache->solution_bins.clear();
    cache->non_zero_solution_bins.clear();
    double scale;
    bool first = true;
    for (const auto& entry : sol_bins) {
      cache->solution_bins.push_back(entry.second);
      if (first) {
        scale = 1 / (1.0 - entry.second);
      } else {
        cache->non_zero_solution_bins.push_back(entry.second * scale);
      }

      first = false;
    }
  }

#define ADD_TIME(NAME) \
  AddTimeObservation(cache->scalar.last_##NAME##_time, &cache->NAME##_times)

  ADD_TIME(iteration);
  ADD_TIME(prepare);
  ADD_TIME(knapsack);
  ADD_TIME(observe);
  ADD_TIME(update);
#undef ADD_TIME

#define ADD_POINT(NAME, NAMES) AddPoint(cache->scalar.NAME, &cache->NAMES)
  ADD_POINT(sum_mix_gap, sum_mix_gaps);
  ADD_POINT(min_loss, min_losses);
  ADD_POINT(max_loss, max_losses);

  ADD_POINT(best_bound, best_bounds);
  ADD_POINT(last_solution_value, solution_values);
#undef ADD_POINT

  if (cache->best_bounds.size() >= 2) {
    AddPoint(cache->best_bounds[0] - cache->best_bounds[1],
             &cache->delta_best_bounds);
  } else {
    AddPoint(0, &cache->delta_best_bounds);
  }

  const double avg_value =
      cache->scalar.sum_solution_value / cache->scalar.num_iterations;
  AddPoint(cache->scalar.best_bound - avg_value, &cache->best_bound_avg_gaps);
  AddPoint(avg_value, &cache->avg_solution_values);
  AddPoint(
      cache->scalar.sum_solution_feasibility / cache->scalar.num_iterations,
      &cache->avg_solution_feasilities);
}

void PlotNarrowLines(const char* label, absl::Span<const float> values) {
  ImGui::PlotLines(label, values.data(), values.size(),
                   /*values_offset=*/0, /*overlay_text=*/nullptr,
                   /*scale_min=*/0.0, /*scale_max=*/FLT_MAX,
                   /*graph_size=*/ImVec2(300, 25));
}
}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const double kFeasEps = absl::GetFlag(FLAGS_feas_eps);

  RandomSetCoverInstance instance = GenerateRandomInstance(
      absl::GetFlag(FLAGS_num_sets), absl::GetFlag(FLAGS_num_values),
      absl::GetFlag(FLAGS_min_set_per_value),
      absl::GetFlag(FLAGS_max_set_per_value));

  GLFWwindow* window;
  {
    int r = setup(&window);
    if (r != 0) {
      return 0;
    }
  }

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // This is only safe because we don't use instance.constraints
  // below.
  SetCoverSolver solver(instance.obj_values,
                        absl::MakeSpan(instance.constraints));

  // XXX: add a way to cancel the thread and actually join it.
  std::thread solver_thread([kFeasEps, &solver] {
    solver.Drive(absl::GetFlag(FLAGS_max_iter), kFeasEps,
                 absl::GetFlag(FLAGS_check_feasible));
  });

  bool text_summary_printed = false;

  struct StateCache last_state;

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    const bool done = solver.IsDone();
    bool any_change = false;

    if (solver.state().mu.TryLock()) {
      if (solver.state().scalar.num_iterations !=
          last_state.scalar.num_iterations) {
        any_change = true;
        last_state.scalar = solver.state().scalar;

        last_state.solution.clear();
        last_state.solution.reserve(solver.state().current_solution.size());
        // XXX: should SMR this up.
        for (double x : solver.state().current_solution) {
          last_state.solution.push_back(x);
        }
      }

      solver.state().mu.Unlock();
    }

    if (any_change && last_state.scalar.num_iterations > 0) {
      UpdateDerivedValues(instance, &last_state);
    }

    if (any_change && done && !text_summary_printed) {
      text_summary_printed = true;

      std::cout << "Violation\n";
      OutputHistogram(std::cout, BinValues(last_state.infeas, 25, kFeasEps),
                      /*step=*/2.5e-2, /*cumulative=*/true);

      std::cout << "\nSolution\n";
      OutputHistogram(std::cout, BinValues(last_state.solution, 25, kFeasEps));

      std::cout << "\nFinal solution: Z=" << last_state.obj_value
                << " infeas=" << last_state.max_violation << "\n";
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::Begin("Summary");

      ImGui::Text("Iteration #%zu", last_state.scalar.num_iterations);
      ImGui::Text("Current avg obj value: %f", last_state.obj_value);
      ImGui::Text("Worst-case constraint infeas: %f", last_state.max_violation);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

      const char* status;
      if (!done) {
        status = "iterating";
      } else if (last_state.scalar.infeasible) {
        status = "infeasible";
      } else if (last_state.scalar.relaxation_optimal) {
        status = "relaxation optimal.";
      } else {
        status = "COMPLETE.";
      }

      ImGui::Text("Status: %s.", status);

      if (ImGui::Button("TestButton")) {
        fprintf(stderr, "Test!\n");
      }

      ImGui::End();
    }

    if (last_state.scalar.num_iterations > 0) {
      {
        ImGui::Begin("Decision variable values");
        ImGui::Text("Decisions (0: %.2f%%)",
                    100 * last_state.solution_bins.front());
        ImGui::PlotHistogram("", last_state.non_zero_solution_bins.data(),
                             last_state.non_zero_solution_bins.size(),
                             /*values_offset=*/0, /*overlay_text=*/nullptr,
                             /*scale_min=*/0.0, /*scale_max=*/FLT_MAX,
                             /*graph_size=*/ImVec2(400, 300));
        ImGui::End();
      }

      {
        ImGui::Begin("Constraints");
        ImGui::Text(
            "Violation (<= eps: %.2f%%)",
            100 * (last_state.infeas_bins.front() + last_state.infeas_bins[1]));
        ImGui::PlotHistogram("", last_state.infeas_bins.data(),
                             last_state.infeas_bins.size(),
                             /*values_offset=*/0, /*overlay_text=*/nullptr,
                             /*scale_min=*/0.0, /*scale_max=*/FLT_MAX,
                             /*graph_size=*/ImVec2(400, 300));

        ImGui::End();
      }

      {
        ImGui::Begin("Timings");

        const double scale = 1000.0 / last_state.scalar.num_iterations;
        ImGui::Text(
            "Avg %.2fms\nprep=%.2fms knap=%.2fms obs=%.2fms upd=%.2fms",
            scale * absl::ToDoubleSeconds(last_state.scalar.total_time),
            scale * absl::ToDoubleSeconds(last_state.scalar.prepare_time),
            scale * absl::ToDoubleSeconds(last_state.scalar.knapsack_time),
            scale * absl::ToDoubleSeconds(last_state.scalar.observe_time),
            scale * absl::ToDoubleSeconds(last_state.scalar.update_time));
        PlotNarrowLines("Iteration", last_state.iteration_times);
        PlotNarrowLines("Prepare", last_state.prepare_times);
        PlotNarrowLines("Knapsack", last_state.knapsack_times);
        PlotNarrowLines("Observe", last_state.observe_times);
        PlotNarrowLines("Update", last_state.update_times);
        ImGui::End();
      }

      {
        ImGui::Begin("Primal / dual");
        ImGui::Text("Best bound %.2f, avg value %.2f, avg feas %.2f",
                    last_state.scalar.best_bound,
                    last_state.scalar.sum_solution_value /
                        last_state.scalar.num_iterations,
                    last_state.scalar.sum_solution_feasibility /
                        last_state.scalar.num_iterations);
        PlotNarrowLines("Delta bound", last_state.delta_best_bounds);
        PlotNarrowLines("Best bound", last_state.best_bounds);
        PlotNarrowLines("Best - avg gap", last_state.best_bound_avg_gaps);
        PlotNarrowLines("Avg sol value", last_state.avg_solution_values);
        PlotNarrowLines("Avg sol feas", last_state.avg_solution_feasilities);
        ImGui::End();
      }
    }

    ImGui::Render();
    int display_w, display_h;
    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwMakeContextCurrent(window);
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
