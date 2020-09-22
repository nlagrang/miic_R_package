#include "environment.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <algorithm>  // std::generate

using Rcpp::as;
using std::string;
using std::vector;
using namespace miic::utility;
using namespace miic::computation;

#define MAGNITUDE_TIES 0.00005f

namespace miic {
namespace structure {

Environment::Environment(
    const Rcpp::List& input_data, const Rcpp::List& arg_list)
    : n_samples(arg_list["n_samples"]),
      n_nodes(arg_list["n_nodes"]),
      data_numeric(n_nodes, n_samples, as<vector<int>>(input_data["factor"])),
      data_double(n_nodes, n_samples, as<vector<double>>(input_data["double"])),
      data_numeric_idx(
          n_nodes, n_samples, as<vector<int>>(input_data["order"])),
      is_continuous(as<vector<int>>(arg_list["is_continuous"])),
      levels(as<vector<int>>(arg_list["levels"])),
      has_na(n_nodes, 0),
      n_eff(as<int>(arg_list["n_eff"])),
      edges(n_nodes, n_nodes),
      orientation(as<bool>(arg_list["orientation"])),
      ori_proba_ratio(as<double>(arg_list["ori_proba_ratio"])),
      propagation(as<bool>(arg_list["propagation"])),
      max_iteration(as<int>(arg_list["max_iteration"])),
      test_mar(as<bool>(arg_list["test_mar"])),
      n_shuffles(as<int>(arg_list["n_shuffles"])),
      conf_threshold(as<double>(arg_list["conf_threshold"])),
      sample_weights(as<vector<double>>(arg_list["sample_weights"])),
      noise_vec(2 * n_samples),
      is_k23(as<bool>(arg_list["is_k23"])),
      degenerate(as<bool>(arg_list["degenerate"])),
      no_init_eta(as<bool>(arg_list["no_init_eta"])),
      half_v_structure(as<int>(arg_list["half_v_structure"])),
      maxbins(as<int>(arg_list["max_bins"])),
      initbins(std::min(30, int(0.5 + cbrt(n_samples)))),
      n_threads(as<int>(arg_list["n_threads"])),
      cache(n_samples),
      verbose(as<bool>(arg_list["verbose"])) {
  auto var_names = as<vector<string>>(arg_list["var_names"]);
  std::transform(var_names.begin(), var_names.end(), std::back_inserter(nodes),
      [](string name) { return Node(name); });

  for (int i = 0; i < n_nodes; ++i) {
    for (int j = 0; j < n_samples; ++j) {
      if (data_numeric(i, j) == -1) {
        has_na[i] = 1;
        break;
      }
    }
  }

  auto consistent_flag = as<std::string>(arg_list["consistent"]);
  if (consistent_flag.compare("orientation") == 0)
    consistent = 1;
  else if (consistent_flag.compare("skeleton") == 0)
    consistent = 2;

  auto latent_flag = as<std::string>(arg_list["latent"]);
  if (latent_flag.compare("yes") == 0) {
    latent = true;
    latent_orientation = true;
  } else if (latent_flag.compare("orientation") == 0)
    latent_orientation = true;

  if (as<string>(arg_list["cplx"]).compare("mdl") == 0) cplx = 0;

  if (n_eff == -1 || n_eff > n_samples)
    n_eff = n_samples;

  if (sample_weights.empty()) {
    double uniform_weight{1};
    if (n_eff != n_samples)
      uniform_weight = static_cast<double>(n_eff) / n_samples;
    sample_weights.resize(n_samples, uniform_weight);
  }

#ifdef _OPENMP
  if (n_threads <= 0) n_threads = omp_get_num_procs();
  omp_set_num_threads(n_threads);
#endif

  for (int i = 0; i < n_nodes; i++) {
    for (int j = 0; j < n_nodes; j++) {
      if ((!is_continuous[i] && levels[i] == n_samples) ||
          (!is_continuous[j] && levels[j] == n_samples)) {
        // If a node is discrete with as many levels as there are samples, its
        // information with other nodes is null.
        edges(i, j).status = 0;
        edges(i, j).status_prev = 0;
      } else {
        // Initialise all other edges.
        edges(i, j).status = 1;
        edges(i, j).status_prev = 1;
      }
    }
  }

  for (int i = 0; i < n_nodes; i++) {
    edges(i, i).status = 0;
    edges(i, i).status_prev = 0;
  }

  std::generate(begin(noise_vec), end(noise_vec),
      []() { return R::runif(-MAGNITUDE_TIES, MAGNITUDE_TIES); });

  readBlackbox(as<vector<vector<int>>>(arg_list["black_box"]));
}

void Environment::readBlackbox(const vector<vector<int>>& node_list) {
  for (const auto& pair : node_list) {
    edges(pair[0], pair[1]).status = 0;
    edges(pair[0], pair[1]).status_prev = 0;
    edges(pair[1], pair[0]).status = 0;
    edges(pair[1], pair[0]).status_prev = 0;
  }
}

}  // namespace structure
}  // namespace miic
