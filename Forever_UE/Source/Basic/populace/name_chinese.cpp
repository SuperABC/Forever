#include "name_chinese.h"

#include "common/utility.h"

#include <cmath>

// 生成给定名时的最大重试次数——照抄老工程同名宏，这次的实现和老工程一样其实从不会真正
// 重试(见GenerateName两参数重载注释)，保留只是为了逐行对照方便。
#define MAX_ALLOCATION_ATTEMPTS 16


using namespace std;

int ChineseName::count = 0;

ChineseName::ChineseName() : id(count++) {
	InitializeSurnames();
	InitializeNames();
}

const char* ChineseName::GetName() {
	name = "中文姓名" + to_string(id);
	return name.c_str();
}

void ChineseName::GetSurname(const string& fullName,
	const function<void(const string&)>& setResult) const {
	if (fullName.empty()) return;

	const unsigned char* s = reinterpret_cast<const unsigned char*>(fullName.data());
	size_t len = fullName.size();

	unsigned char first = s[0];
	size_t charLen = 0;

	if (first < 0x80) charLen = 1;
	else if ((first & 0xE0) == 0xC0) charLen = 2;
	else if ((first & 0xF0) == 0xE0) charLen = 3;
	else if ((first & 0xF8) == 0xF0) charLen = 4;
	else return;

	if (len < charLen) return;

	for (size_t i = 1; i < charLen; ++i) {
		if ((s[i] & 0xC0) != 0x80) return;
	}

	setResult(fullName.substr(0, charLen));
}

void ChineseName::GenerateName(bool allowMale, bool allowFemale, bool allowNeutral,
	const function<void(const string&)>& setResult) const {
	// 姓氏按"前面的姓更常见"做非均匀采样：N=surnames.size()，从[0,N²)里均匀取randVal，
	// idx=floor(sqrt(randVal))本身就是非均匀的(idx越大概率越高)，surnameIdx=N-1-idx把
	// 这个偏向反过来，让数组靠前(常见姓氏排在前面，见InitializeSurnames)的项被抽中的概率
	// 远高于靠后的——不存权重表，靠数组顺序+这个sqrt反索引隐式实现"常见姓氏更常见"。
	int randVal = GetRandom(static_cast<int>(surnames.size() * surnames.size()));
	int idx = static_cast<int>(sqrt(static_cast<float>(randVal)));
	int surnameIdx = static_cast<int>(surnames.size()) - 1 - idx;
	GenerateName(surnames[surnameIdx], allowMale, allowFemale, allowNeutral, setResult);
}

void ChineseName::GenerateName(const string& surname,
	bool allowMale, bool allowFemale, bool allowNeutral,
	const function<void(const string&)>& setResult) const {
	int nameList[3] = {
		allowMale ? static_cast<int>(maleNames.size()) : 0,
		allowFemale ? static_cast<int>(femaleNames.size()) : 0,
		allowNeutral ? static_cast<int>(neutralNames.size()) : 0,
	};

	// 1个字8/9的样本落在GetRandom(9)==0，2个字的样本占其余8/9——两字名远比单字名常见，
	// 照抄老工程的比例。
	int nameLength = 1 + static_cast<int>(static_cast<bool>(GetRandom(9)));
	int listLength = nameList[0] + nameList[1] + nameList[2];

	if (listLength == 0) {
		// 三个词库都不允许时兜底成只用中性词库——和老工程同样的兜底规则。
		nameList[2] = static_cast<int>(neutralNames.size());
		listLength = nameList[2];
	}
	if (listLength == 0) return; // 词库本身是空的(理论上不会发生)，生成失败

	string givenName;
	int attempt = 0;
	while (true) {
		if (attempt++ > MAX_ALLOCATION_ATTEMPTS) return;
		givenName.clear();
		for (int i = 0; i < nameLength; ++i) {
			int pick = GetRandom(listLength);
			if (pick < nameList[0]) {
				givenName += maleNames[pick];
			}
			else {
				pick -= nameList[0];
				if (pick < nameList[1]) {
					givenName += femaleNames[pick];
				}
				else {
					pick -= nameList[1];
					givenName += neutralNames[pick];
				}
			}
		}
		setResult(surname + givenName);
		return;
	}
}

void ChineseName::InitializeSurnames() {
	surnames = {
		"王", "李", "张", "刘", "陈", "杨", "赵", "黄", "周", "吴",
		"徐", "孙", "胡", "朱", "高", "林", "何", "郭", "马", "罗",
		"梁", "宋", "郑", "谢", "韩", "唐", "冯", "于", "董", "萧",
		"程", "曹", "袁", "邓", "许", "傅", "沈", "曾", "彭", "吕",
		"苏", "卢", "蒋", "蔡", "贾", "丁", "魏", "薛", "叶", "阎",
		"余", "潘", "杜", "戴", "夏", "钟", "汪", "田", "任", "姜",
		"范", "方", "石", "姚", "谭", "廖", "邹", "熊", "金", "陆",
		"郝", "孔", "白", "崔", "康", "毛", "邱", "秦", "江", "史",
		"顾", "侯", "邵", "孟", "龙", "万", "段", "雷", "钱", "汤",
		"尹", "黎", "易", "常", "武", "乔", "贺", "赖", "龚", "文",
		"庞", "樊", "兰", "殷", "施", "陶", "洪", "翟", "安", "颜",
		"倪", "严", "牛", "温", "芦", "季", "俞", "章", "鲁", "葛",
		"伍", "韦", "申", "尤", "毕", "聂", "丛", "焦", "向", "柳",
		"邢", "路", "岳", "齐", "沿", "梅", "莫", "庄", "辛", "管",
		"祝", "左", "涂", "谷", "祁", "时", "舒", "耿", "牟", "卜",
		"路", "詹", "关", "苗", "凌", "费", "纪", "靳", "盛", "童",
		"欧", "甄", "项", "曲", "成", "游", "阳", "裴", "席", "卫",
		"查", "屈", "鲍", "位", "覃", "霍", "翁", "隋", "植", "甘",
		"景", "薄", "单", "包", "司", "柏", "宁", "柯", "阮", "桂",
		"闵", "都", "解", "强", "柴", "华", "车", "冉", "房", "边"
	};
}

void ChineseName::InitializeNames() {
	maleNames = {
		"伟", "强", "勇", "军", "磊", "洋", "超", "健", "明", "涛",
		"鹏", "亮", "刚", "杰", "峰", "建", "波", "浩", "凯", "宇",
		"帅", "晨", "龙", "剑", "博", "飞", "达", "宏", "斌", "楷",
		"辉", "栋", "锋", "彬", "旭", "豪", "翔", "坤", "瀚", "圣",
		"诚", "伦", "勋", "扬", "渊", "弘", "翰", "野", "岩", "武",
		"雄", "侠", "岳", "雷", "威", "猛", "韬", "滔", "骅", "臣",
		"勃", "铂", "恺", "铠", "锴", "城", "烨", "麟", "铉", "桁",
		"嵩", "钧", "峪", "坚", "钢", "业", "忠", "晟", "松", "振",
		"虎", "器", "骑", "武", "劲", "佰", "柱", "基", "晋", "瑾",
	};

	femaleNames = {
		"楠", "霖", "诗", "书", "晗", "艺", "雅", "慧", "敏", "梅",
		"兰", "菊", "霞", "露", "霜", "雪", "荧", "珠", "畅", "淇",
		"斯", "欣", "芳", "娜", "静", "丽", "艳", "柔", "凝", "诗",
		"娟", "玲", "燕", "颖", "洁", "琳", "婷", "莹", "琼", "倩",
		"琪", "瑜", "蕾", "雯", "怡", "璇", "妍", "玥", "璐", "娅",
		"琦", "晶", "瑶", "蓓", "仪", "荷", "丹", "蓉", "滢", "虹",
		"蕊", "薇", "菁", "梦", "苑", "婕", "馨", "韵", "园", "卿",
		"芸", "菲", "淑", "惠", "贞", "娴", "妃", "香", "芬", "翠",
		"婉", "姝", "姣", "媚", "嫣", "妩", "姗", "婵", "娥", "妤",
		"媛", "妙", "佳", "俏", "舞", "莲", "茉", "莉", "玫", "瑰",
		"桂", "桃", "樱", "棠", "蔷", "珏", "珮", "环", "璎", "碧",
		"茗", "靓", "凤", "珑", "霏", "绯", "芷", "琅", "柳", "萱",
		"菈", "络", "其", "七", "茹", "绣", "琢", "妞", "汶", "朦",
	};

	neutralNames = {
		"华", "阳", "苏", "昊", "清", "铭", "越", "皓", "祺", "航",
		"涵", "辰", "奕", "煊", "宸", "昱", "轩", "贤", "智", "远",
		"哲", "祥", "仁", "雨", "世", "昌", "泰", "泽", "非", "歌",
		"云", "懿", "维", "风", "运", "佑", "炫", "舟", "南", "音",
		"民", "鸣", "志", "树", "林", "森", "朴", "梁", "柯", "桂",
		"文", "德", "才", "学", "礼", "隆", "能", "君", "琴", "净",
		"正", "端", "方", "谨", "谦", "恭", "敬", "信", "义", "秋",
		"孝", "廉", "毅", "果", "决", "聪", "珊", "珀", "春", "夏",
		"捷", "观", "朗", "潇", "洒", "逸", "群", "卓", "越", "冬",
		"凡", "俊", "秀", "英", "士", "子", "儒", "小", "少", "古",
		"山", "川", "河", "海", "江", "湖", "泉", "溪", "流", "今",
		"石", "沙", "原", "田", "园", "木", "柏", "竹", "丰", "逢",
		"花", "草", "叶", "枝", "根", "苗", "禾", "谷", "率", "琛",
		"麦", "日", "灵", "巧", "晴", "画", "棋", "晓", "欢", "寒",
		"月", "星", "光", "煌", "灿", "福", "禄", "寿", "喜", "财",
		"宝", "玉", "金", "银", "瑞", "吉", "庆", "康", "伊", "亚",
		"顺", "利", "通", "发", "展", "兴", "旺", "盛", "应", "映",
		"贵", "平", "和", "谐", "美", "好", "善", "良", "真", "实",
		"子", "志", "永", "立", "国", "绍", "继", "承", "传", "宜",
		"耀", "显", "启", "开", "创", "新", "革", "改", "进", "修",
		"治", "保", "守", "护", "卫", "定", "大", "可", "舒", "影",
		"广", "深", "厚", "高", "远", "长", "久", "思", "英", "雅",
		"聪", "澜", "纯", "昭", "冰", "爽", "羽", "希", "听", "然",
		"杨", "枫", "桐", "桑", "梓", "杉", "曦", "晞", "昀", "墨",
		"夕", "晚", "夜", "祯", "祎", "贺", "悦", "愉", "暗", "朋",
		"荣", "安", "宁", "睿", "悦", "乐", "嘉", "笑", "钰", "彤",
		"一", "心", "亦", "若", "凌", "之", "景", "灵", "白", "桦",
		"忆", "元", "初", "问", "雁", "如", "寻", "千", "尔", "友",
		"谷", "以", "念", "冷", "依", "惜", "度", "怀", "天", "解",
		"从", "语", "访", "落", "欲", "闲", "惊", "眠", "港", "结",
		"笔", "崖", "杭", "堂", "熙", "化", "含", "煜", "临", "宣",
		"为", "唯", "朝", "简", "渐", "名", "结", "暄", "瑄", "渲",
		"灰", "晖", "东", "续", "许", "须", "绪", "栩", "叙", "相",
		"速", "肃", "成", "程", "乘", "情", "青", "倾", "轮", "红",
		"论", "讯", "迅", "循", "跃", "阅", "源", "愿", "圆", "洪",
		"缘", "男", "生", "胜", "升", "行", "意", "易", "益", "鸿",
		"异", "翼", "谊", "驿", "育", "予", "浴", "裕", "于", "与",
		"绚", "先", "献", "衔", "羡", "至", "支", "直", "知", "滨",
		"制", "芝", "执", "颂", "淞", "喆", "及", "计", "缤", "汇",
		"极", "纪", "积", "济", "际", "棘", "太", "台", "由", "游",
		"优", "悠", "留", "周", "州", "洲", "研", "言", "严", "颜",
		"延", "焰", "宴", "任", "韧", "师", "识", "始", "关", "毕",
		"加", "家", "甲", "洛", "步", "布", "普", "蒲", "茂", "矛",
		"起", "期", "齐", "奇", "无", "务", "尽", "锦", "津", "拜",
		"翘", "楚", "纹", "温", "闻", "二", "鑫", "幸", "中", "融",
	};
}
