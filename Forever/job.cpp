#include "job.h"


using namespace std;

string HotelCleanerJob::GetId() {
	return "hotel_cleaner";
}

string HotelCleanerJob::GetType() const {
	return "hotel_cleaner";
}

string HotelCleanerJob::GetName() const {
	return "酒店保洁";
}

vector<string> HotelCleanerJob::GetScripts() const {
	return { "../Resources/scripts/jobs/hotel_cleaner.json" };
}
