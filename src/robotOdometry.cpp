/**
 * \file robotOdometry.cpp
 * \date 01/01/2012
 * \author dmarquez
 * \author agonzale
 * 
 * \ingroup rtslam
 */

#include "jmath/jblas.hpp"
#include "jmath/ublasExtra.hpp"
#include "boost/numeric/ublas/operation.hpp"
#include "boost/numeric/ublas/matrix_proxy.hpp"
#include "boost/numeric/ublas/vector_proxy.hpp"
#include "rtslam/quatTools.hpp"
#include "rtslam/robotAbstract.hpp"
#include "rtslam/robotOdometry.hpp"

namespace jafar {
	namespace rtslam {
		using namespace std;
		using namespace jmath;

		/**
		 * Remote constructor from remote map.
		 * \param _map the remote map
		 */
		RobotOdometry::RobotOdometry(const map_ptr_t _mapPtr) :
			RobotAbstract(_mapPtr, RobotOdometry::size(),
			              RobotOdometry::size_control(),
			              RobotOdometry::size_perturbation()) {
			constantPerturbation = false;
			type = ODOMETRY;
		}

		RobotOdometry::RobotOdometry(const simulation_t dummy,
		    const map_ptr_t _mapPtr) :
			RobotAbstract(FOR_SIMULATION, _mapPtr, RobotOdometry::size(),
			              RobotOdometry::size_control(),
			              RobotOdometry::size_perturbation()) {
			constantPerturbation = true;
			type = ODOMETRY;
		}

		void RobotOdometry::move_func(const vec & _x, const vec & _u,
		    const vec & _n, const double _dt, vec & _xnew, mat & _XNEW_x,
		    mat & _XNEW_u) {

			using namespace jblas;
			using namespace ublas;

			/*
			 * This motion model is defined by:
			 * The state vector, x = [p q] = [x y z, qw qx qy qz], of size 7.
			 * The odometry input vector, given by odometry sensors, u = [dx, dv] = [dxx dxy dxz,qx qy qz].
			 *
			 * -p += dx <- position
			 * -q += v2q(dv) <- quaternion
			 * dx : position increment 		- dx = [dxx dxy dxz]
			 * dv : orientation increment - dv = [qx qy qz]
			 *
			 * The Jacobian XNEW_x is built with :
			 *   var    |  p       q
			 *      pos |  0       3
			 *   -------+---------------
			 *   p   0  |    PNEW_x
			 *   q   3  |  0     QNEW_q
			 * -------------------------
			 *
			 * The Jacobian XNEW_n is built with :
			 *   var    |  dx       dv
			 *      pos |  0         3
			 *   -------+------------------
			 *   p   0  |PNEW_dx     0
			 *   q   3  |  0     QNEW_dv
			 * ----------------------------
			 */
			
			//variables
			mat PNEW_x(3, 7);

			// split robot state vector
			vec3 p;
			vec4 q;
			splitState(_x, p, q);

			// split control vector
			// position increment
			// orientation increment
			vec3 dx, dv;
			splitControl(_u, dx, dv);

			// position update
			vec3 pnew;
			vec4 qnew;
			quaternion::eucFromFrame(_x, dx, pnew, PNEW_x, PNEW_dx); 
			
			//quaternion update
			vec4 qdv;
			quaternion::v2q(dv, qdv, QDV_dv); //orientation increment to quaternion with jacobians
			quaternion::qProd(q, qdv, qnew, QNEW_q, QNEW_qdv);
			
			QNEW_dv = prod(QNEW_qdv, QDV_dv);
			
			unsplitState(pnew, qnew, _xnew);
			
			_XNEW_x.clear();
			subrange(_XNEW_x, 0, 3, 0, 7) = PNEW_x;
			subrange(_XNEW_x, 3, 7, 3, 7) = QNEW_q;

			_XNEW_u.clear();
			subrange(_XNEW_u, 0, 3, 0, 3) = PNEW_dx;
			subrange(_XNEW_u, 3, 7, 3, 6) = QNEW_dv;			
		}
			
		void RobotOdometry::init_func(const vec & _x, const vec & _u, vec & _xnew) {
			
			using namespace jblas;
			using namespace ublas;
			
			// split robot state vector
			vec3 p;
			vec4 q;
			splitState(_x, p, q);
			
			unsplitState(p, q, _xnew); //FIXME temporary solution to copy the initial state
		}
		
		void RobotOdometry::writeLogHeader(kernel::DataLogger& log) const
		{
			std::ostringstream oss; oss << "Robot " << id();
			log.writeComment(oss.str());
			
			log.writeLegendTokens("time");
			log.writeLegendTokens("absx absy absz");
			log.writeLegendTokens("x y z");
			log.writeLegendTokens("qw qx qy qz");
			log.writeLegendTokens("yaw pitch roll");

			log.writeLegendTokens("sig_x sig_y sig_z");
			log.writeLegendTokens("sig_qw sig_qx sig_qy sig_qz");
			log.writeLegendTokens("sig_yaw sig_pitch sig_roll");

		}
		
		void RobotOdometry::writeLogData(kernel::DataLogger& log) const
		{
			jblas::vec euler_x(3);
			jblas::sym_mat euler_P(3,3);
			quaternion::q2e(ublas::subrange(state.x(), 3, 7), ublas::project(state.P(), ublas::range(3, 7), ublas::range(3,7)), euler_x, euler_P);
			
			log.writeData(self_time);
			for(int i = 0 ; i < 3 ; ++i) log.writeData(state.x()(i)+origin_sensors(i)-origin_export(i));
			for(int i = 0 ; i < 7 ; ++i) log.writeData(state.x()(i));
			for(int i = 0 ; i < 3 ; ++i) log.writeData(euler_x(2-i));

			for(int i = 0 ; i < 7 ; ++i) log.writeData(sqrt(state.P()(i,i)));
			for(int i = 0 ; i < 3 ; ++i) log.writeData(sqrt(euler_P(2-i,2-i)));
		}
	}
}
