// M_Terrain 的 Custom 节点主体(build_terrain_material.py 原样贴进材质)。数据布局见 ForeverTerrainFrameworkComponent.md "地形材质"一节。
// 输入：WP(绝对世界坐标cm) Cam(相机世界坐标cm) VN(顶点法线,世界) VC(顶点色,R=水深编码)
//       DiffArr(漫反射 BC7 sRGB) NrmArr(法线 BC5，RG=xy，DirectX约定) MaskArr(遮罩 BC7，ORM: R=AO G=粗糙度；单独的粗糙度图只有G有效)
//       CovArr(覆盖图数组) TypeTab/LayerTab(float表) Noise(4通道噪声)
//       MapInfo(宽,高,地形种类数,覆盖图切片数) WaterEnc(水深编码min,range) T0/T1/T2(调参向量)
// 输出：return=BaseColor；附加输出 NormalOut(世界空间) RoughOut AOOut。
// 注意：HLSL里near/far/step/noise是保留字或内建函数名，变量不要用这些名字。

float3 n0 = normalize(VN);
float2 mel = WP.xy * 0.001;                  // element坐标
float dist = length(WP - Cam) * 0.01;        // 到相机的米数
float3 dpx = ddx(WP);                        // 世界坐标的屏幕空间导数(分支外算好，分支里用SampleGrad)
float3 dpy = ddy(WP);
float2 mapSize = MapInfo.xy;
int typeCount = (int)(MapInfo.z + 0.5);
int sliceCount = (int)(MapInfo.w + 0.5);

float4 nzA = Noise.SampleLevel(NoiseSampler, mel / T2.y, 0);
float4 nzB = Noise.SampleLevel(NoiseSampler, mel / (T2.y * 16.0), 0);

// ---- 覆盖度：噪声扭曲采样位置，使平滑的高斯边界变得不规则
float2 cuv = (mel + (nzA.rg - 0.5) * (2.0 * T0.x)) / mapSize;
float2 cdx = ddx(mel) / mapSize;
float2 cdy = ddy(mel) / mapSize;
float cov[8];
[unroll] for (int k = 0; k < 8; k++) cov[k] = 0.0;
[unroll] for (int s = 0; s < 2; s++)
{
	if (s < sliceCount)
	{
		float4 c = CovArr.SampleGrad(CovArrSampler, float3(cuv, s), cdx, cdy);
		cov[s * 4 + 0] = c.x; cov[s * 4 + 1] = c.y; cov[s * 4 + 2] = c.z; cov[s * 4 + 3] = c.w;
	}
}

// 每种地形叠一点独立噪声，再把权重"锐化"：只保留离最大者band以内的种类，过渡带变窄、格内大部分像素只剩1~2种
float cmax = 0.0;
[unroll] for (int i0 = 0; i0 < 8; i0++)
{
	float nv = (i0 < 4) ? nzA[min(i0, 3)] : nzB[max(i0 - 4, 0)];   // 两边都会被求值：下标夹住，否则unroll后出现越界字面下标，cook的FXC报X3504
	cov[i0] = max(cov[i0] + (nv - 0.5) * T0.y, 0.0);
	cmax = max(cmax, cov[i0]);
}
float band = max(T0.z, 0.01);
float w[8];
float wsum = 1e-5;
[unroll] for (int i1 = 0; i1 < 8; i1++)
{
	w[i1] = saturate((cov[i1] - (cmax - band)) / band);
	wsum += w[i1];
}
float wmax = 0.0;
[unroll] for (int i2 = 0; i2 < 8; i2++) { w[i2] /= wsum; wmax = max(wmax, w[i2]); }

float farF = smoothstep(T1.x, T1.y, dist);              // 远处：不采样层贴图，用平均色
float nrmFade = 1.0 - smoothstep(T1.z, T1.w, dist);     // 细节法线随距离淡出
float triBlend = 1.0 - smoothstep(T2.x * 0.6, T2.x, dist); // 三向投影只在近处
// 中距离起丢掉次要地形(权重低于阈值)，省采样；阈值不超过最大权重的0.9，保证至少留一种
float wmin = min(saturate((dist - 25.0) / 80.0) * 0.3, wmax * 0.9);
float wsum2 = 1e-5;
[unroll] for (int i3 = 0; i3 < 8; i3++) { if (w[i3] < wmin) w[i3] = 0.0; wsum2 += w[i3]; }
[unroll] for (int i4 = 0; i4 < 8; i4++) w[i4] /= wsum2;

float depthM = VC.r * WaterEnc.y + WaterEnc.x;          // 米，正=在水下
float slope = 1.0 - n0.z;                               // 0=平地，越大越陡

float3 colAcc = 0.0;
float3 nrmAcc = 0.0;
float roughAcc = 0.0;
float aoAcc = 0.0;

[loop] for (int ti = 0; ti < 8; ti++)
{
	if (ti >= typeCount || w[ti] <= 0.0005) continue;
	float4 tt0 = TypeTab.Load(int3(0, ti, 0));             // layerA, layerB, selector, -
	float4 tt1 = TypeTab.Load(int3(1, ti, 0));             // range0, range1
	int selector = (int)round(tt0.z);
	float sv = (selector == 1) ? (slope + (nzA.b - 0.5) * 0.05) : ((selector == 2) ? (depthM + (nzA.b - 0.5) * 0.2) : 0.0);
	float tB = (selector == 0 || tt0.y < -0.5) ? 0.0 : smoothstep(tt1.x, tt1.y, sv);

	[loop] for (int slot = 0; slot < 2; slot++)
	{
		float wk = w[ti] * ((slot == 0) ? (1.0 - tB) : tB);
		if (wk <= 0.0005) continue;
		int L = (int)round((slot == 0) ? tt0.x : tt0.y);

		float3 lc = float3(0.5, 0.5, 0.5);
		float3 ln = n0;
		float lr = 0.8;
		float lao = 1.0;
		if (L >= 0)
		{
			float4 l0 = LayerTab.Load(int3(0, L, 0));          // tile(cm), triplanar, 遮罩贴图R是否为AO
			bool hasAo = l0.z > 0.5;
			// 远处的"平均色/平均粗糙度"：直接采样1x1的最低mip(层贴图是cook后的压缩数据，CPU读不出像素，不预先算平均色)
			float3 avgCol = float3(0.5, 0.5, 0.5);
			float avgRough = 0.8;
			if (farF > 0.001)
			{
				float3 avgUv = float3(WP.xy / l0.x, (float)L);
				avgCol = DiffArr.SampleLevel(DiffArrSampler, avgUv, 10.0).rgb;
				avgRough = MaskArr.SampleLevel(MaskArrSampler, avgUv, 10.0).g;
			}
			lc = avgCol;
			lr = avgRough;
			if (farF < 0.999)
			{
				float tile = l0.x;
				float2 uvZ = WP.xy / tile;
				float2 gzx = dpx.xy / tile;
				float2 gzy = dpy.xy / tile;
				float4 dZ = DiffArr.SampleGrad(DiffArrSampler, float3(uvZ, L), gzx, gzy);
				float4 nZ = NrmArr.SampleGrad(NrmArrSampler, float3(uvZ, L), gzx, gzy);
				float4 mZ = MaskArr.SampleGrad(MaskArrSampler, float3(uvZ, L), gzx, gzy);
				float3 tnZ = float3(nZ.rg * 2.0 - 1.0, 0.0);
				tnZ.z = sqrt(saturate(1.0 - dot(tnZ.xy, tnZ.xy)));
				float3 wnZ = float3(tnZ.xy + n0.xy, tnZ.z * n0.z);   // whiteout：Z向投影
				float3 dcol = dZ.rgb;
				float drough = mZ.g;
				float dao = hasAo ? mZ.r : 1.0;
				float3 dn = wnZ;
				if (l0.y > 0.5 && triBlend > 0.001)
				{
					// 陡峭层近处三向投影(Ben Golus whiteout)：和Z向投影按triBlend混合
					float3 bw = pow(abs(n0), 4.0);
					bw /= (bw.x + bw.y + bw.z + 1e-5);
					float3 bz = lerp(float3(0.0, 0.0, 1.0), bw, triBlend);
					float2 uvX = WP.zy / tile;
					float2 uvY = WP.xz / tile;
					float4 dX = DiffArr.SampleGrad(DiffArrSampler, float3(uvX, L), dpx.zy / tile, dpy.zy / tile);
					float4 nX = NrmArr.SampleGrad(NrmArrSampler, float3(uvX, L), dpx.zy / tile, dpy.zy / tile);
					float4 mX = MaskArr.SampleGrad(MaskArrSampler, float3(uvX, L), dpx.zy / tile, dpy.zy / tile);
					float4 dY = DiffArr.SampleGrad(DiffArrSampler, float3(uvY, L), dpx.xz / tile, dpy.xz / tile);
					float4 nY = NrmArr.SampleGrad(NrmArrSampler, float3(uvY, L), dpx.xz / tile, dpy.xz / tile);
					float4 mY = MaskArr.SampleGrad(MaskArrSampler, float3(uvY, L), dpx.xz / tile, dpy.xz / tile);
					float3 tnX = float3(nX.rg * 2.0 - 1.0, 0.0);
					tnX.z = sqrt(saturate(1.0 - dot(tnX.xy, tnX.xy)));
					tnX = float3(tnX.xy + n0.zy, tnX.z * n0.x);
					float3 tnY = float3(nY.rg * 2.0 - 1.0, 0.0);
					tnY.z = sqrt(saturate(1.0 - dot(tnY.xy, tnY.xy)));
					tnY = float3(tnY.xy + n0.xz, tnY.z * n0.y);
					dn = tnX.zyx * bz.x + tnY.xzy * bz.y + wnZ * bz.z;
					dcol = dX.rgb * bz.x + dY.rgb * bz.y + dZ.rgb * bz.z;
					drough = mX.g * bz.x + mY.g * bz.y + mZ.g * bz.z;
					dao = hasAo ? (mX.r * bz.x + mY.r * bz.y + mZ.r * bz.z) : 1.0;
				}
				dn = normalize(dn);
				lc = lerp(dcol, avgCol, farF);
				lr = lerp(drough, avgRough, farF);
				ln = normalize(lerp(dn, n0, farF));
				lao = lerp(dao, 1.0, farF);
			}
		}
		colAcc += wk * lc;
		nrmAcc += wk * ln;
		roughAcc += wk * lr;
		aoAcc += wk * lao;
	}
}

float3 nFinal = normalize(lerp(n0, normalize(nrmAcc + n0 * 1e-4), nrmFade));
colAcc *= lerp(0.88, 1.12, nzB.a);           // 低频明暗变化，打散大片平地的重复感
NormalOut = nFinal;
RoughOut = saturate(roughAcc);
AOOut = saturate(aoAcc);
return colAcc;
