#include "Element/FurnitureCollision.h"

#include "Components/PrimitiveComponent.h"

namespace {
	// 网格局部系里的一块：x从背面(-X，靠墙那一侧)量起，y从-Y一侧量起，z从底面量起；单位是"设计米"，
	// 乘unit(这件网格每设计米合多少局部单位)换成局部坐标。x1/y1/z1给负数表示"到包围盒那一侧的尽头"。
	struct FPart {
		double x0, x1, y0, y1, z0, z1;
	};

	FBox MakeBox(const FPart& part, const FVector& boundsMin, const FVector& boundsMax, double unit) {
		auto lo = [unit](double value, double minEdge) { return minEdge + value * unit; };
		auto hi = [unit](double value, double minEdge, double maxEdge) { return value < 0.0 ? maxEdge : FMath::Min(maxEdge, minEdge + value * unit); };
		FVector minPoint(lo(part.x0, boundsMin.X), lo(part.y0, boundsMin.Y), lo(part.z0, boundsMin.Z));
		FVector maxPoint(hi(part.x1, boundsMin.X, boundsMax.X), hi(part.y1, boundsMin.Y, boundsMax.Y), hi(part.z1, boundsMin.Z, boundsMax.Z));
		minPoint = minPoint.ComponentMin(boundsMax);
		return FBox(minPoint, maxPoint.ComponentMax(minPoint));
	}
}

void FurnitureCollision::GetBoxes(const FString& meshKey, const FVector& boundsMin, const FVector& boundsMax, TArray<FBox>& outBoxes) {
	int32 slash = INDEX_NONE;
	meshKey.FindLastChar(TEXT('/'), slash);
	const FString name = meshKey.Mid(slash + 1);
	const FVector size = boundsMax - boundsMin;
	const double sizeY = size.Y;

	// 下面的尺寸都取自Tools/Furniture/make_furniture.py里拼这件家具的长方体(米；-X=背面，+X=正面，原点在底面中心)。
	// unit用这一类家具固定不变的那个设计尺寸反推，不依赖网格的导入单位。
	TArray<FPart> parts;
	double unit = 100.0;
	if (name.StartsWith(TEXT("Bed_"))) {
		// 床：总高1.05；床头板在背面厚0.07、高到顶；床身(床架+床垫+被子枕头)高到0.60。
		unit = size.Z / 1.05;
		parts.Add({ 0.0, 0.07, 0.0, -1.0, 0.0, -1.0 });  // 床头板
		parts.Add({ 0.07, -1.0, 0.0, -1.0, 0.0, 0.60 }); // 床身
	}
	else if (name.StartsWith(TEXT("Sofa")) || name.StartsWith(TEXT("Armchair"))) {
		// 沙发/扶手椅：进深0.90、总高0.88；靠背在背面厚0.20、高到顶；扶手各宽0.15、顶0.62；坐垫顶0.50。
		unit = size.Z / 0.88;
		const double width = unit > 0.0 ? sizeY / unit : 0.0;
		parts.Add({ 0.0, 0.20, 0.0, -1.0, 0.0, -1.0 });                    // 靠背
		parts.Add({ 0.20, -1.0, 0.15, FMath::Max(0.15, width - 0.15), 0.0, 0.50 }); // 坐垫
		parts.Add({ 0.20, 0.87, 0.0, 0.15, 0.0, 0.62 });                   // 扶手(-Y)
		parts.Add({ 0.20, 0.87, FMath::Max(0.0, width - 0.15), -1.0, 0.0, 0.62 }); // 扶手(+Y)
	}
	else if (name.StartsWith(TEXT("Chair")) || name.StartsWith(TEXT("DiningChair"))) {
		// 椅子/餐椅：进深0.44；靠背在背面厚0.04、高到顶；座面顶0.47(座面+椅腿算一块)。
		unit = size.X / 0.44;
		parts.Add({ 0.0, 0.04, 0.0, -1.0, 0.0, -1.0 });  // 靠背
		parts.Add({ 0.04, -1.0, 0.0, -1.0, 0.0, 0.47 }); // 座面和椅腿
	}
	else if (name.StartsWith(TEXT("TvStand"))) {
		// 电视柜：进深0.41；柜体高0.45；电视屏是柜顶中间的一块薄板(厚约0.07，宽为柜宽的62%)，高到顶。
		unit = size.X / 0.41;
		const double width = unit > 0.0 ? sizeY / unit : 0.0;
		parts.Add({ 0.0, -1.0, 0.0, -1.0, 0.0, 0.45 });                              // 柜体
		parts.Add({ 0.17, 0.24, width * 0.19, width * 0.81, 0.45, -1.0 });           // 电视屏
	}
	else if (name.StartsWith(TEXT("Toilet"))) {
		// 马桶：进深0.54；水箱在背面厚0.18、高到顶；座高0.44。
		unit = size.X / 0.54;
		parts.Add({ 0.0, 0.18, 0.0, -1.0, 0.0, -1.0 });  // 水箱
		parts.Add({ 0.18, -1.0, 0.0, -1.0, 0.0, 0.44 }); // 座
	}
	else if (name.StartsWith(TEXT("Shower"))) {
		// 淋浴：0.90见方；底盘只有0.06高不挡，人能走进去；只挡背面中间的立管和花洒臂(进深0.30、宽0.14)。
		unit = size.X / 0.90;
		const double width = unit > 0.0 ? sizeY / unit : 0.0;
		parts.Add({ 0.0, 0.30, width * 0.5 - 0.07, width * 0.5 + 0.07, 0.0, -1.0 });
	}
	// 其余(衣柜、桌子、茶几、床头柜、冰箱、橱柜、灶台、水槽、吊柜、洗脸盆、浴缸、洗衣机、书架、绿植、落地灯、鞋柜、斗柜……)：
	// 一个等于包围盒的盒子。地毯这类薄片由调用方按kMinBlockingHeight整件跳过。

	if (parts.Num() == 0 || unit <= 0.0) {
		outBoxes.Add(FBox(boundsMin, boundsMax));
		return;
	}
	for (const FPart& part : parts) outBoxes.Add(MakeBox(part, boundsMin, boundsMax, unit));
}

void FurnitureCollision::ApplyBlockingResponse(UPrimitiveComponent* component) {
	if (!component) return;
	// 只挡人/物理物体/射线/相机(玩家和市民是Pawn通道；本项目Pawn预设不理会Visibility，武器射线用ECC_Pawn)，其余全忽略。
	component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	component->SetCollisionObjectType(ECC_WorldStatic);
	component->SetCollisionResponseToAllChannels(ECR_Ignore);
	component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	component->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	// 必须Block ECC_Visibility：Mannequin的脚部IK控制绑定(CR_Mannequin_FootIK)用Visibility球形射线找地面，
	// 盒子若忽略它，玩家站上盒顶时射线穿过盒子打到地板，IK把脚和骨盆往下拉到地板——胶囊在盒顶、小白人却插在家具里。
	component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// 必须Block ECC_Camera：弹簧臂的探测通道默认是ECC_Camera，忽略它相机就会穿进家具。
	component->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
}
