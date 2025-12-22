#include "job.h"


using namespace std;

std::string HotelCleanerJob::GetId() {
	return "hotel_cleaner";
}

std::string HotelCleanerJob::GetType() const {
	return "hotel_cleaner";
}

std::string HotelCleanerJob::GetName() const {
	return "酒店保洁";
}

std::vector<std::string> HotelCleanerJob::GetScripts() const {
	return { "../Resources/scripts/jobs/hotel_cleaner.json" };
}
