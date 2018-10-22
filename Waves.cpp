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
	wave_w = m;
	wave_h = n;

    mVertexCount = m*n;
    mTriangleCount = (m - 1)*(n - 1) * 2;

    mTimeStep = dt;
    mSpatialStep = dx;

    float d = damping*dt + 2.0f;
    float e = (speed*speed)*(dt*dt) / (dx*dx);
    mK1 = (damping*dt - 2.0f) / d;
    mK2 = (4.0f - 8.0f*e) / d;
    mK3 = (2.0f*e) / d;

	mEarth.resize(m*n);
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
            mCurrSolution[i*n + j] = XMFLOAT3(x, 0.0f, z);
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
	return wave_w;
}

int Waves::ColumnCount()const
{
	return wave_h;
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
	return wave_w*mSpatialStep;
}

float Waves::Depth()const
{
	return wave_h*mSpatialStep;
}

void Waves::Update(float tt)
{
	std::vector<XMFLOAT3> vec = 
	{
		XMFLOAT3(sinf(tt * 0.13f) * 20.f, 0.f, cosf(tt * 0.1f) * 20.f),
		XMFLOAT3(sinf(tt / 10 / 3.14) * 50.f + wave_w / 3.f, 0.f, fabsf(100.f - fmodf(tt, 200.f)) - 50.f + wave_h / 3.f),
		XMFLOAT3(sinf(tt / 10 / 3.14) * 50.f + wave_w / 4.f, 0.f, fabsf(100.f - fmodf(tt, 200.f)) - 50.f - wave_h / 5.f),
		XMFLOAT3(sinf(tt / 10 / 3.14) * 50.f - wave_w / 1.4f, 0.f, fabsf(100.f - fmodf(tt, 200.f)) - 50.f - wave_h / 2.3f)
	};

	
	concurrency::parallel_for(0, wave_w, [tt, vec, this](int i)
	{
		for (int j = 0; j < wave_h; ++j)
		{
			float r = 0;
			auto& P = mCurrSolution[i*wave_h + j].y;
			P = -1.f;

			float a = 1.f;
			for (const auto& O : vec)
			{
				r = sqrtf(powf(i - O.x - wave_w / 2.f, 2) + powf(-O.y, 2) + powf(j - O.z - wave_h / 2.f, 2));
				P = std::max(0.1f * sinf(r / sqrtf(a) + tt * 10.f) / log(r + 3.14f), P);
				a += 0.2f;
			}
		}
	});

	//std::swap(mPrevSolution, mCurrSolution);

	concurrency::parallel_for(1, wave_w - 1, [this](int i)
	{
		for (int j = 1; j < wave_h - 1; ++j)
		{
			float l = mCurrSolution[i*wave_h + j - 1].y;
			float r = mCurrSolution[i*wave_h + j + 1].y;
			float t = mCurrSolution[(i - 1)*wave_h + j].y;
			float b = mCurrSolution[(i + 1)*wave_h + j].y;
			mNormals[i*wave_h + j].x = -r + l;
			mNormals[i*wave_h + j].y = 2.0f*mSpatialStep;
			mNormals[i*wave_h + j].z = b - t;

			XMVECTOR n = XMVector3Normalize(DirectX::XMLoadFloat3(&mNormals[i*wave_h + j]));
			DirectX::XMStoreFloat3(&mNormals[i*wave_h + j], n);

			mTangentX[i*wave_h + j] = XMFLOAT3(2.0f*mSpatialStep, r - l, 0.0f);
			XMVECTOR T = XMVector3Normalize(DirectX::XMLoadFloat3(&mTangentX[i*wave_h + j]));
			DirectX::XMStoreFloat3(&mTangentX[i*wave_h + j], T);
		}
	});

}