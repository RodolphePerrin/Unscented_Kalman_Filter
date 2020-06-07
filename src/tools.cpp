#include "tools.h"
#include <iostream>
using Eigen::VectorXd;
using std::vector;


Tools::Tools() {}

Tools::~Tools() {}



VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
                              const vector<VectorXd> &ground_truth) {
      VectorXd RMSE(4);
      RMSE<< 0,0,0,0;
      int n = estimations.size();
      
      if(n != ground_truth.size())
      {
          std::cout<<"Error in CalculateRMSE: sizes of estimations and ground truth vectors differs."<<std::endl;
          return RMSE;
      }
      
      if (n==0)
      {
         std::cout<<"Error in CalculateRMSE: sizes of estimations and ground truth are null."<<std::endl;
          return RMSE;
      }
      
      //Compute RMSE
      
      for (int t= 0; t<n; t++ )
      {
          VectorXd diff = estimations[t]-ground_truth[t];
          diff= diff.array()*diff.array();
          
          RMSE += diff;
      }
      RMSE = RMSE/n;
      RMSE = RMSE.array().sqrt();
      
      return RMSE;
  
}
