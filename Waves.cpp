//***************************************************************************************
// Waves.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "Waves.h"
#include <ppl.h>
#include <algorithm>
#include <vector>
#include <cassert>

using namespace DirectX;

Waves::Waves(int m, int n, float dx, float dt, float speed, float damping)
{
    mNumRows = m;
    mNumCols = n;

    mVertexCount = m*n;
    mTriangleCount = (m - 1)*(n - 1) * 2;

    mTimeStep = dt;
    mSpatialStep = dx;

    float d = damping*dt + 2.0f;
    float e = (speed*speed)*(dt*dt) / (dx*dx);
    mK1 = (damping*dt - 2.0f) / d;
    mK2 = (4.0f - 8.0f*e) / d;
    mK3 = (2.0f*e) / d;

    mPrevSolution.resize(m*n);
    mCurrSolution.resize(m*n);
    mNormals.resize(m*n);
    mTangentX.resize(m*n);

    // Generate grid vertices in system memory.

    float halfWidth = (n - 1)*dx*0.5f;
    float halfDepth = (m - 1)*dx*0.5f;
    for(int i = 0; i < m; ++i)
    {
        float z = halfDepth - i*dx;
        for(int j = 0; j < n; ++j)
        {
            float x = -halfWidth + j*dx;

            mPrevSolution[i*n + j] = XMFLOAT3(x, 0.0f, z);
            mCurrSolution[i*n + j] = XMFLOAT3(x, 0.45f, z);
            mNormals[i*n + j] = XMFLOAT3(0.0f, 1.0f, 0.0f);
            mTangentX[i*n + j] = XMFLOAT3(1.0f, 0.0f, 0.0f);
        }
    }
}

Waves::~Waves()
{
}

int Waves::RowCount()const
{
	return mNumRows;
}

int Waves::ColumnCount()const
{
	return mNumCols;
}

int Waves::VertexCount()const
{
	return mVertexCount;
}

int Waves::TriangleCount()const
{
	return mTriangleCount;
}

float Waves::Width()const
{
	return mNumCols*mSpatialStep;
}

float Waves::Depth()const
{
	return mNumRows*mSpatialStep;
}

void Waves::Update(float dt)
{

	concurrency::parallel_for(1, mNumRows - 1, [this](int i)
	{
		for (int j = 1; j < mNumCols - 1; ++j)
		{
			float l = mCurrSolution[i*mNumCols + j - 1].y;
			float r = mCurrSolution[i*mNumCols + j + 1].y;
			float t = mCurrSolution[(i - 1)*mNumCols + j].y;
			float b = mCurrSolution[(i + 1)*mNumCols + j].y;
			mNormals[i*mNumCols + j].x = -r + l;
			mNormals[i*mNumCols + j].y = 2.0f*mSpatialStep;
			mNormals[i*mNumCols + j].z = b - t;

			XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mNormals[i*mNumCols + j]));
			XMStoreFloat3(&mNormals[i*mNumCols + j], n);

			mTangentX[i*mNumCols + j] = XMFLOAT3(2.0f*mSpatialStep, r - l, 0.0f);
			XMVECTOR T = XMVector3Normalize(XMLoadFloat3(&mTangentX[i*mNumCols + j]));
			XMStoreFloat3(&mTangentX[i*mNumCols + j], T);
		}
	});

}