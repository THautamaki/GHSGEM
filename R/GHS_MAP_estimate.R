
GHS_MAP_estimate <- function(X, p0 = 0, verbose = 0, tol = 1e-4, max_iterations = 200) {
  n <- nrow(X)
  p <- ncol(X)
  if (p0 == 0) {
    p0 <- p - 1
  }
  if (p > 200) {
    tau_f <- (p0/n)^1.5*(100/(p*(p-1)/2))
  }
  else {
    tau_f <- (p0/n)^1.5*(50/(p*(p-1)/2))
  }
  GHSGEM_est <- graphical_horseshoe_map_Cpp(X, verbose = verbose, tol = tol, max_iter = max_iterations,
                                            fixed_tau = tau_f)
  return(GHSGEM_est)
}