#include "dialog.h"


using namespace std;

Option::Option(std::string condition, std::string option,
	vector<Dialog*> dialogs, vector<const Change*> changes) :
	condition(move(condition)), option(move(option)), dialogs(move(dialogs)), changes(move(changes)) {

}

Option::~Option() {

}

const std::string& Option::GetCondition() const {
	return condition;
}

const std::string& Option::GetOption() const {
	return option;
}

vector<Dialog*> Option::GetDialogs() const {
	return dialogs;
}

vector<const Change*> Option::GetChanges() const {
	return changes;
}

Section::Section(std::string speaker, std::string content, std::string label, std::string voice) :
	branch(false),
	speakerExpr(move(speaker)), contentExpr(move(content)), labelExpr(move(label)), voiceExpr(move(voice)),
	speaking(),
	ownerScript(nullptr) {

}

Section::Section(vector<Option> options) :
	branch(true),
	speakerExpr(), contentExpr(), labelExpr(), voiceExpr(),
	speaking(),
	options(move(options)),
	ownerScript(nullptr) {

}

Section::~Section() {

}

bool Section::IsBranch() const {
	return branch;
}

void Section::EvaluateText(const ScriptContext& context) {
	speaking = {
		ToString(EvaluateExpression(speakerExpr, context)),
		ToString(EvaluateExpression(contentExpr, context)),
		ToString(EvaluateExpression(labelExpr, context)),
		ToString(EvaluateExpression(voiceExpr, context))
	};
}

tuple<string, string, string, string> Section::GetSpeaking() const {
	return speaking;
}

vector<Option> Section::GetOptions() const {
	return options;
}

void Section::SetOwnerScript(Script* script) {
	ownerScript = script;
}

Script* Section::GetOwnerScript() const {
	return ownerScript;
}

Dialog::Dialog() :
	condition() {

}

Dialog::~Dialog() {

}

void Dialog::AddDialog(std::string speaker, std::string content, std::string label, std::string voice) {
	list.emplace_back(move(speaker), move(content), move(label), move(voice));
}

void Dialog::AddDialog(vector<Option> options) {
	list.emplace_back(move(options));
}

vector<Section> Dialog::GetDialogs() const {
	return list;
}

void Dialog::SetCondition(std::string condition) {
	this->condition = move(condition);
}

const std::string& Dialog::GetCondition() const {
	return condition;
}
