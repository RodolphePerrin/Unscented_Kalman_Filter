#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

#define EPS 0.001

using Eigen::MatrixXd;
using Eigen::VectorXd;
using namespace std;
/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;
    
  // initializes state dimension
  n_x_ = 5;

  // initial state vector
  x_ = VectorXd(n_x_);

  // initial covariance matrix
  P_ = MatrixXd(n_x_, n_x_);
  P_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 0, 1;

  // Process noise standard deviation longitudinal acceleration in m/s^2
    std_a_ = 1.5;

  // Process noise standard deviation yaw acceleration in rad/s^2
    std_yawdd_ = 0.5;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  lambda_ = 3 - n_x_;

  //Augmented state dimension (add process noise)
  n_aug_ = n_x_ +2;
    
  //Sigma points dimension
  n_sig_ = 2 * n_aug_ + 1;

  // Initialize weights.
  weights_ = VectorXd(n_sig_);
  weights_.fill(0.5 / (n_aug_ + lambda_));
  weights_(0) = lambda_ / (lambda_ + n_aug_);
    
  // Initialize measurement noice covariance matrix ( for radar : rho, phi, phi_point - non-linear)
  R_radar_ = MatrixXd(3, 3);
  R_radar_ << std_radr_*std_radr_, 0, 0,
              0, std_radphi_*std_radphi_, 0,
              0, 0,std_radrd_*std_radrd_;
    
  // Initialize measurement noice covariance matrix ( for lidar : x,y - linear).
  R_lidar_ = MatrixXd(2, 2);
  R_lidar_ << std_laspx_*std_laspx_,0,
              0,std_laspy_*std_laspy_;
  cout<<"UKF initialized"<<endl;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
    //First measurement
    if(!is_initialized_)
    {
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      double rho = meas_package.raw_measurements_[0]; // range
      double phi = meas_package.raw_measurements_[1]; // bearing
      double rho_dot = meas_package.raw_measurements_[2]; // velocity of rh
      double x = rho * cos(phi);
      double y = rho * sin(phi);
      double vx = rho_dot * cos(phi);
      double vy = rho_dot * sin(phi);
      double v = sqrt(vx * vx + vy * vy);
      x_ << x, y, v, 0, 0;
    }
    if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      double x = meas_package.raw_measurements_[0]; // x position measurement
      double y = meas_package.raw_measurements_[1]; // y position measurement
      x_<<x,y,0,0,0;
    }
    
    //first timestamp
     time_us_ = meas_package.timestamp_;
    //declare that the process has been initialized.
     is_initialized_ = true;
     cout<<"Measurement initialized"<<endl;
     return;
    }
        double dt = (meas_package.timestamp_ - time_us_)/ 1000000.0;
        time_us_ = meas_package.timestamp_;
         
        //Prediction step
        Prediction(dt);
        cout<<"Predicted"<<endl;
        //Adding measurement to the prediction (radar or lidar)
        if(meas_package.sensor_type_ == MeasurementPackage::RADAR)
        {
            cout<<"Update R"<<endl;
            UpdateRadar(meas_package);
            cout<<"Updated R !!!!"<<endl;
        }
        if(meas_package.sensor_type_ == MeasurementPackage::LASER)
        {
             cout<<"Update L"<<endl;
            UpdateLidar(meas_package);
             cout<<"Updated L !!!!"<<endl;
        }
}

//Predict sigma points of the augmented state and the state covariance matrix.
void UKF::Prediction(double delta_t) {
  //Generate sigma points.
    //Generate augmented state adding process noise variables.
    VectorXd x_aug = VectorXd(n_aug_);
    x_aug.head(5) = x_;
    x_aug(5) = 0;
    x_aug(6) = 0;
    
    //Generate augmented state covariance matrix
    MatrixXd P_aug = MatrixXd(n_aug_,n_aug_);
    P_aug.fill(0.0);
    P_aug.topLeftCorner(n_x_,n_x_) = P_;
    P_aug(5,5) = std_a_*std_a_;
    P_aug(6,6) = std_yawdd_*std_yawdd_;
    
    //Create Sigma Points.
    MatrixXd Xsig_aug_ = GenerateSigmaPoints(x_aug, P_aug, lambda_, n_sig_);
    //Predict Sigma Points
    Xsig_pred_ = PredictSigmaPoints(Xsig_aug_, delta_t, n_x_, n_sig_, std_a_, std_yawdd_ );
    //Predict Mean and Covariance
    
    //Predicted State mean
    x_.fill(0.);
    for( int i = 0; i < n_sig_; i++ )
      x_ = x_ + weights_(i)*Xsig_pred_.col(i);
    
    //Predicted State covariance matrix
    P_.fill(0.0);
    for (int i = 0; i < n_sig_; i++) {  //iterate over sigma points

      // state difference
      VectorXd x_diff = Xsig_pred_.col(i) - x_;
      //angle normalization to keep it inside [-M_PI, M_Pi]
      NormalizeAngleOnComponent(x_diff, 3);

      P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
    }
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
    
    //First step : Predict measurement using sigmap points and passing them in the measurement space. Here,
    // the measurement is linear so we can just retrieve x and y values from each sigma points in the state space and add them
    // directly into the Zsig matrix.
    int n_z = 2;
    MatrixXd Zsig = Xsig_pred_.block(0,0,n_z,n_sig_);
    cout<<"Zsig"<<endl;
    //mean predicted measurement
    VectorXd z_pred = VectorXd(n_z);
    z_pred.fill(0.0);
    for (int i=0; i < n_sig_; i++) {
        z_pred = z_pred + weights_(i) * Zsig.col(i);
    }

    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z,n_z);
    S.fill(0.0);
    for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points
      //residual
      VectorXd z_diff = Zsig.col(i) - z_pred;

      S = S + weights_(i) * z_diff * z_diff.transpose();
    }
    //add measurement noise covariance matrix
    S = S + R_lidar_;
    
    //create matrix for cross correlation between sigma points in state space and measurement space.
    MatrixXd Tc = MatrixXd(n_x_, n_z);

    Tc.fill(0.0);
    for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points

      //residual
      VectorXd z_diff = Zsig.col(i) - z_pred;

      // state difference
      VectorXd x_diff = Xsig_pred_.col(i) - x_;

      Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
    }
    
    // Second step : Update state
      // Incoming Lidar measurement
      VectorXd z = meas_package.raw_measurements_;

      //Kalman gain K;
      MatrixXd K = Tc * S.inverse();

      //residual
      VectorXd z_diff = z - z_pred;

      //update state mean and covariance matrix
      x_ = x_ + K * z_diff;
      P_ = P_ - K*S*K.transpose();

      //NIS Lidar Update
      NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;
    
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  
    //First step : Predict measurement using sigmap points and passing them in the measurement space.
    int n_z = 3;
    // 1. Predict measurement
    MatrixXd Zsig = MatrixXd(n_z, n_sig_);
    //transform sigma points into measurement space
    for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points

      // extract values for better readibility
      double p_x = Xsig_pred_(0,i);
      double p_y = Xsig_pred_(1,i);
      double v  = Xsig_pred_(2,i);
      double yaw = Xsig_pred_(3,i);

      double v1 = cos(yaw)*v;
      double v2 = sin(yaw)*v;

      // measurement model
      Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
      Zsig(1,i) = atan2(p_y,p_x);                                 //phi
      Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
    }
    
    //mean predicted measurement
      VectorXd z_pred = VectorXd(n_z);
      z_pred.fill(0.0);
      for (int i=0; i < n_sig_; i++) {
          z_pred = z_pred + weights_(i) * Zsig.col(i);
      }

      //measurement covariance matrix S
      MatrixXd S = MatrixXd(n_z,n_z);
      S.fill(0.0);
      for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points
        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;

        //angle normalization (make sure angle phi is inside [-M_pi, M_Pi].)
        NormalizeAngleOnComponent(z_diff, 1);

        S = S + weights_(i) * z_diff * z_diff.transpose();
      }

      //add measurement noise covariance matrix
      S = S + R_radar_;

      // 2. Update state
      // Incoming radar measurement
      VectorXd z = meas_package.raw_measurements_;

      //create matrix for cross correlation Tc
      MatrixXd Tc = MatrixXd(n_x_, n_z);

      Tc.fill(0.0);
      for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points

        //residual
        VectorXd z_diff = Zsig.col(i) - z_pred;
        //angle normalization (make sure angle phi is inside [-M_PI, M_Pi]).
        NormalizeAngleOnComponent(z_diff, 1);

        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        //angle normalization ((make sure angle yaw is inside [-M_PI, M_Pi]),
        NormalizeAngleOnComponent(x_diff, 3);

        Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
      }

      //Kalman gain K;
      MatrixXd K = Tc * S.inverse();

      //residual
      VectorXd z_diff = z - z_pred;

      //angle normalization
      NormalizeAngleOnComponent(z_diff, 1);

      //update state mean and covariance matrix
      x_ = x_ + K * z_diff;
      P_ = P_ - K*S*K.transpose();

      //NIS Update
      NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;
    
}


MatrixXd UKF::GenerateSigmaPoints(VectorXd x, MatrixXd P, double lambda, int n_sig) {
   /**
    *   Generate sigma points:
    *  @param x : State vector.
    *  @param P : Covariance matrix.
    *  @param lambda: Sigma points spreading parameter.
    *  @param n_sig: Sigma points dimension.
    */
    int n = x.size();
    //create sigma point matrix
    MatrixXd Xsig = MatrixXd( n, n_sig_ );

    //calculate A, square root of P (formula given by Udacity using Eigen library)
    MatrixXd A = P.llt().matrixL();
    Xsig.col(0) = x;

  double sqrt_coeff = sqrt(lambda + n);
  //Generate 2*n +1 sigma points as desired.
  for (int i = 0; i < n; i++){
      Xsig.col( i + 1 ) = x + sqrt_coeff * A.col(i);
      Xsig.col( i + 1 + n ) = x - sqrt_coeff * A.col(i);
  }
  return Xsig;
}
        

MatrixXd UKF::PredictSigmaPoints(MatrixXd Xsig, double delta_t, int n_x, int n_sig, double nu_am, double nu_yawdd) {
 /**
  * Predits sigma points.
  * @param Xsig : Sigma points to predict (each column is a predicted state correspind to one sigma point to predict)
  * @param delta_t : Time between k and k+1 in s
  * @param n_x : State dimension.
  * @param n_sig : Sigma points dimension.
  * @param nu_am : Process noise standard deviation longitudinal acceleration in m/s^2
  * @param nu_yawdd : Process noise standard deviation yaw acceleration in rad/s^2
   
  We are reusing here the process model equations to predict the sigma points. This wil give us the predicted sigma points in the predicted state, allowing us to predict the mean and covariance based on these points and the weights.
  */
  MatrixXd Xsig_pred = MatrixXd(n_x, n_sig);
  //predict sigma points
  for (int i = 0; i< n_sig; i++)
  {
    //extract values for better readability
    double p_x = Xsig(0,i);
    double p_y = Xsig(1,i);
    double v = Xsig(2,i);
    double yaw = Xsig(3,i);
    double yawd = Xsig(4,i);
    double nu_a = Xsig(5,i);
    double nu_yawdd = Xsig(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw)) + 0.5*nu_a*delta_t*delta_t * cos(yaw);
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) ) + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw) + 0.5*nu_a*delta_t*delta_t * cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw) + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    }

    double v_p = v + nu_a*delta_t;
    double yaw_p = yaw + yawd*delta_t + 0.5*nu_yawdd*delta_t*delta_t;
    double yawd_p = yawd + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred(0,i) = px_p;
    Xsig_pred(1,i) = py_p;
    Xsig_pred(2,i) = v_p;
    Xsig_pred(3,i) = yaw_p;
    Xsig_pred(4,i) = yawd_p;
  }

  return Xsig_pred;
}
/**
 *  Normalized the component `index` of the vector `vector` to be inside [-M_PI, M_PI] interval. In our case, we want yaw to always be inside the interval.
 */
void UKF::NormalizeAngleOnComponent(VectorXd vector, int index) {
  while (vector(index)> M_PI) vector(index)-=2.*M_PI;
  while (vector(index)<-M_PI) vector(index)+=2.*M_PI;
}
        

