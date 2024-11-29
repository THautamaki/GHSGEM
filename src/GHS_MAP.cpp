#include <RcppArmadillo.h>
// [[Rcpp::depends(RcppArmadillo)]]

using namespace std;
using namespace std::chrono;
using namespace arma;
using namespace Rcpp;

// [[Rcpp::export]]
List graphical_horseshoe_map_Cpp(const mat& X, double t_alpha = 1.0, double t_beta = 1.0, 
                                 double tol = 1e-4, int max_iter = 200, const string& diff_type = "relative",
                                 int verbose = 1, bool alt_kappa = true, double fixed_tau = 0) {
    auto start_time = high_resolution_clock::now();
    int iter = 1;
    double diff = 1.0;
    int n = X.n_rows;
    int p = X.n_cols;
    mat S = trans(X) * X;

    vec diffs = zeros(max_iter);
    vec tau_sqs = zeros(max_iter);

    mat Sigma = eye(p, p);
    mat Omega = eye(p, p);
    mat Omega_last_iter = eye(p, p);
    mat Lambda_sq = ones(p, p);
    mat Nu = ones(p, p);

    double tau_sq = fixed_tau > 0 ? fixed_tau : 1.0;
    double tau_sq_shape = t_alpha + p * (p - 1) / 4.0;
    
    while (diff > tol && iter <= max_iter) {
        for (int i = 0; i < p; i++) {
            uvec col_vec = linspace<uvec>(i, i, 1);
            uvec ind_noi = linspace<uvec>(0, p-1, p);
            ind_noi.shed_row(i);
            mat Sigma_11 = Sigma(ind_noi, ind_noi);
            vec Sigma_12 = Sigma(ind_noi, col_vec);
            double Sigma_22 = Sigma(i, i);
            vec lambda_sq_12 = Lambda_sq(ind_noi, col_vec);
            vec nu_12 = Nu(ind_noi, col_vec);
            vec s_12 = S(ind_noi, col_vec);
            double s_22 = S(i, i);
            // Calculate gamma, omega_12, omega_22, lambda_12^2 and nu_12
            double gamma = (n / 2.0 + 1) / (s_22 / 2.0);
            mat Omega_11_inv = Sigma_11 - Sigma_12 * trans(Sigma_12) / Sigma_22;
            mat C_inv = s_22 * Omega_11_inv + diagmat(1.0 / (lambda_sq_12 * tau_sq));

            vec beta = solve(C_inv, -s_12);
            vec omega_12 = beta;
            //Rcout << beta;
            //Rcout << gamma + trans(beta) * Omega_11_inv * beta << "\n";
            double omega_22 = as_scalar(gamma + trans(beta) * Omega_11_inv * beta);
            vec lambda_scale = 1.0 / nu_12 + square(omega_12) / (2 * tau_sq);
            lambda_sq_12 = lambda_scale / 2.0;
            vec nu_scale = ones(p-1) + 1.0 / lambda_sq_12;
            nu_12 = nu_scale / 2.0;

            // Update Omega, Sigma, Lambda^2 and Nu
            Omega(i, i) = omega_22;
            Omega(col_vec, ind_noi) = trans(omega_12);
            Omega(ind_noi, col_vec) = omega_12;

            vec Omega_inv_temp = Omega_11_inv * beta;
            Sigma(ind_noi, ind_noi) = Omega_11_inv + Omega_inv_temp * trans(Omega_inv_temp) / gamma;
            vec Sigma_12_new = -Omega_inv_temp / gamma;
            Sigma(col_vec, ind_noi) = trans(Sigma_12_new);
            Sigma(ind_noi, col_vec) = Sigma_12_new;
            Sigma(i, i) = 1.0 / gamma;

            Lambda_sq(col_vec, ind_noi) = trans(lambda_sq_12);
            Lambda_sq(ind_noi, col_vec) = lambda_sq_12;
            Nu(col_vec, ind_noi) = trans(nu_12);
            Nu(ind_noi, col_vec) = nu_12;
        }
        // Update tau^2 and xi
        if (fixed_tau == 0) {
            mat omega_ltri = trimatl(Omega, -1);
            mat lambda_sq_ltri = trimatl(Lambda_sq, -1);
            mat ltri_mat = square(omega_ltri) / (2.0 * lambda_sq_ltri);
            ltri_mat.replace(datum::nan, 0);
            double tau_sq_scale = t_beta + accu(ltri_mat);
            tau_sq = tau_sq_scale / (tau_sq_shape + 1.0);
        }

        if (diff_type == "relative") {
            diff = norm(Omega - Omega_last_iter, "fro") / norm(Omega_last_iter, "fro");
        } else {
            diff = norm(Omega - Omega_last_iter, "fro");
        }

        diffs(iter-1) = diff;
        if (verbose > 1) {
            auto lap_time = high_resolution_clock::now();
            double elap_time = duration_cast<microseconds>(lap_time - start_time).count();
            Rcout << "Iteration: " << iter << ". Elapsed time: " << elap_time/1000000 << " s. Difference: " << diff << endl;
        } else if (verbose > 0 && iter % 10 == 0) {
			      auto lap_time = high_resolution_clock::now();
            double elap_time = duration_cast<microseconds>(lap_time - start_time).count();
            Rcout << "Iteration: " << iter << ". Elapsed time: " << elap_time/1000000 << " s. Difference: " << diff << endl;
		}

        tau_sqs(iter-1) = tau_sq;
        Omega_last_iter = Omega;

        iter++;
    }

    // Final kappa and theta calculations
    mat kappa = ones(p, p) / (1.0 + Lambda_sq);
    if (alt_kappa) {
        kappa = ones(p, p) / (1.0 + n * tau_sq * Lambda_sq);
    }
    mat theta = ones(p, p) - kappa;
    umat inds = find(theta >= 0.5);
    theta.elem(inds) = mat(size(inds), fill::value(1));
    umat inds2 = find(theta < 0.5);
    theta.elem(inds2) = mat(size(inds2), fill::value(0));
    theta.diag(0) = zeros(p);

    auto end_time = high_resolution_clock::now();
    double elap_time = duration_cast<microseconds>(end_time - start_time).count();

    if (verbose >= 0) {
        Rcout << "Total iterations: " << iter - 1 << ". Elapsed time: " << elap_time/1000000 << " s. Final difference: " << diff << endl;
    }
    List results = List::create(_["Sigma_est"] = Sigma, _["Omega_est"] = Omega, _["Theta"] = theta,
                                _["Kappa"] = kappa, _["Taus"] = tau_sqs, _["diffs"] = diffs,
                                _["iters"] = iter - 1, _["tot_time"] = elap_time);
    return results;
}