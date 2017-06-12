// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2015 Qianyi Zhou <Qianyi.Zhou@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "TransformationEstimation.h"

#include <Eigen/Geometry>
#include <Core/Geometry/PointCloud.h>

namespace three{

double TransformationEstimationPointToPoint::ComputeRMSE(
		const PointCloud &source, const PointCloud &target,
		const CorrespondenceSet &corres) const
{
	if (corres.empty()) return 0.0;
	double err = 0.0;
	for (const auto &c : corres) {
		err += (source.points_[c[0]] - target.points_[c[1]]).squaredNorm();
	}
	return std::sqrt(err / (double)corres.size());
}

Eigen::Matrix4d TransformationEstimationPointToPoint::ComputeTransformation(
		const PointCloud &source, const PointCloud &target,
		const CorrespondenceSet &corres) const
{
	if (corres.empty()) return Eigen::Matrix4d::Identity();
	Eigen::MatrixXd source_mat(3, corres.size());
	Eigen::MatrixXd target_mat(3, corres.size());
	for (size_t i = 0; i < corres.size(); i++) {
		source_mat.block<3, 1>(0, i) = source.points_[corres[i][0]];
		target_mat.block<3, 1>(0, i) = target.points_[corres[i][1]];
	}
	return Eigen::umeyama(source_mat, target_mat, with_scaling_);
}

double TransformationEstimationPointToPlane::ComputeRMSE(
		const PointCloud &source, const PointCloud &target,
		const CorrespondenceSet &corres) const
{
	if (corres.empty() || target.HasNormals() == false) return 0.0;
	double err = 0.0, r;
	for (const auto &c : corres) {
		r = (source.points_[c[0]] - target.points_[c[1]]).dot(
				target.normals_[c[1]]);
		err = r * r;
	}
	return std::sqrt(err / (double)corres.size());
}

Eigen::Matrix4d TransformationEstimationPointToPlane::ComputeTransformation(
		const PointCloud &source, const PointCloud &target,
		const CorrespondenceSet &corres) const
{
	if (corres.empty() || target.HasNormals() == false)
		return Eigen::Matrix4d::Identity();
	Eigen::Matrix<double, 6, 6> ATA;
	Eigen::Matrix<double, 6, 1> ATb;
	Eigen::Matrix<double, 6, 1> A_r;
	double r;
	ATA.setZero();
	ATb.setZero();
	for (size_t i = 0; i < corres.size(); i++) {
		const Eigen::Vector3d &vs = source.points_[corres[i][0]];
		const Eigen::Vector3d &vt = target.points_[corres[i][1]];
		const Eigen::Vector3d &nt = target.normals_[corres[i][1]];
		r = (vs - vt).dot(nt);
		A_r.block<3, 1>(0, 0) = vs.cross(nt);
		A_r.block<3, 1>(3, 0) = nt;
		ATA += A_r * A_r.transpose();
		ATb += A_r * r;
	}
	auto llt_solver = ATA.llt();
	Eigen::Matrix<double, 6, 1> x = -llt_solver.solve(ATb);
	Eigen::Matrix4d transformation;
	transformation.setIdentity();
	transformation.block<3, 3>(0, 0) =
			(Eigen::AngleAxisd(x(2), Eigen::Vector3d::UnitZ()) *
			Eigen::AngleAxisd(x(1), Eigen::Vector3d::UnitY()) *
			Eigen::AngleAxisd(x(0), Eigen::Vector3d::UnitX())).matrix();
	transformation.block<3, 1>(0, 3) = x.block<3, 1>(3, 0);
	return transformation;
}

}	// namespace three