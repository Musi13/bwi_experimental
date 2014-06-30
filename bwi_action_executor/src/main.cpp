#include "actions/Action.h"
#include "actions/ActionFactory.h"
#include "actions/LogicalNavigation.h"
#include "actions/Load.h"
#include "actions/Unload.h"
#include "actions/Greet.h"
#include "actions/Order.h"
#include "actions/ChooseFloor.h"

#include "kr_interface.h"

#include <ros/ros.h>

#include <iostream>
#include <boost/concept_check.hpp>
#include <boost/graph/graph_concepts.hpp>

#include <list>

using namespace bwi_actexec;
using namespace std;



std::list<Action *> computePlan(const std::string& ,unsigned int);
bool checkPlan(const std::list<Action *> & plan, const std::string& goalSpecification);
std::list<Action *> repairPlan(const std::list<Action *> & plan, const std::string& goalSpecification, unsigned int max_changes);

int main(int argc, char** argv) {

	ros::init(argc, argv, "bwi_action_executor");
	ros::NodeHandle n;

	ros::Rate loop(10);

	//noop updates the fluents with the current position
	LogicalNavigation setInitialState("noop");
	setInitialState.run();

	//forever

	//TODO wait for a goal


	//string goal = ":- not served(alice,coffee,n).";
	//string goal = ":- not at(f3_410,n).";
	string goal;
    n.getParam("/bwi_action_executor/goal", goal);
    std::cerr << "Plan Goal is: "<< goal << endl;
	const unsigned int MAX_N = 40;
    std::cerr << "Comuputing inital plan";
	std::list<Action *> plan = computePlan(goal, MAX_N);
    std::cerr << "Plan finished computing";
    std::list< Action *>::iterator planit1 = plan.begin();
    std::string ss1;
    int mycounter = 0;
    for (; planit1 != plan.end(); ++planit1){
        ss1 = ss1 + (*planit1)->toASP(0) + " ";
    }

    for (; planit1 != plan.end(); ++planit1){
        if ( (*planit1) == NULL ) {
            mycounter++;
        }
    }
    cerr << "[BWI_ACTION_EXECUTOR] Counter of nulls: " << mycounter << endl;
    

    std::cerr << "Plan Length is: " << plan.size() << std::endl;
    std::cerr << "The list of all the actions is: " << ss1 << std::endl;



    
    
	if(plan.empty())
	
	
		throw runtime_error("The plan to achieve " + goal + " is empty!");
	Action * currentAction = plan.front();
	plan.pop_front();
	
	unsigned int executed = 0;

	while (ros::ok()) {

		ros::spinOnce();
		if (!currentAction->hasFinished()) {
			cerr << "Executing the current action: " << currentAction->toASP(0) << endl;
			currentAction->run();
		}
		else { // Move on to next action
            cerr << "Action " << currentAction->toASP(0) << "hasfinished()" << std::endl; 
			delete currentAction;

			executed++;
	
			cerr << "Forward Projecting Plan to Check Validity..." << endl;
			bool valid = checkPlan(plan,goal);
			if(!valid) {
				
				cerr << "Forward projection failed, trying repair" << endl;
				
				//plan repair
				int max_changes = min((MAX_N-executed-plan.size()), plan.size());
				std::list<Action *> repairedPlan = repairPlan(plan, goal, max_changes);

				//delete the old plan
				list<Action *>::iterator actIt = plan.begin();
				for(; actIt != plan.end(); ++actIt)
					delete *actIt;	
				plan.clear();


				if (repairedPlan.empty()) {
					cerr << "replanning..." << endl;
					plan = computePlan(goal,MAX_N);
					if(plan.empty())
						throw runtime_error("The plan to achieve " + goal + " is empty!");
				}
				else {
					cerr << "repair success" << endl;
					plan = repairedPlan;
				}


                // Print new plan
                std::list< Action *>::iterator planit2 = plan.begin();
                std::string ss2;
                for (; planit2 != plan.end(); ++planit2){
                    ss2 = ss2 + (*planit2)->toASP(0) + " ";
                }
                std::cerr << "Plan Length is: " << plan.size() << std::endl;
                std::cerr << "The list of all the actions is: " << ss2 << std::endl;

			}

			currentAction = plan.front();
			plan.pop_front();

		}
		loop.sleep();
	}

	return 0;
}



std::list<Action *> computePlan(const std::string& goalSpecification, unsigned int max_n) {
	

	bwi_kr::AnswerSetMsg answerSet;

	for (int i=0; i<max_n && !answerSet.satisfied ; ++i) {
		
		stringstream goal;
		goal << goalSpecification << endl;
		goal << "#hide." << endl;
		
		ActionFactory::ActionMap::const_iterator actIt = ActionFactory::actions().begin();
		for( ; actIt != ActionFactory::actions().end(); ++actIt) {
			//the last parameter is always the the step number
			goal << "#show " << actIt->second->getName() << "/" << actIt->second->paramNumber() + 1 << "." << endl;
		}

		answerSet = kr_query(goal.str(),i,"planQuery.asp");

	}

	vector<bwi_kr::Predicate> &preds = answerSet.predicates;

	vector<Action *> planVector(preds.size());
	for (int j=0 ; j<preds.size() ; ++j) {
		
		Action *act = ActionFactory::byName(preds[j].name);
		act->init(preds[j].parameters);
		planVector[preds[j].timeStep] = act;
	}

	list<Action *> plan(planVector.begin(),planVector.end());
	
	return plan;
}

bool checkPlan(const std::list<Action *> & plan, const std::string& goalSpecification) {

	stringstream queryStream;

	list<Action *>::const_iterator planIt = plan.begin();
	
	for(unsigned int timeStep = 0; planIt != plan.end(); ++planIt, ++timeStep) {
		queryStream << (*planIt)->toASP(timeStep) << "." << endl;
	}
	
	queryStream << goalSpecification << endl;
	
	bwi_kr::AnswerSetMsg answerSet = kr_query(queryStream.str(),plan.size(), "checkPlan.asp");

	return answerSet.satisfied;

}

std::list<Action *> repairPlan(const std::list<Action *> & plan, const std::string& goalSpecification, unsigned int max_changes) {
	
	cerr << "repairing..." << "maximum number of changes is " << max_changes << endl;

	bwi_kr::AnswerSetMsg answerSet;

	for (int i=1; (i<=max_changes) ; ++i) {
		
		int insert_N = i; //number of actions inserted to old plan
		int delete_N = 0; //number of actions deleted from old plan
		
		std::list<Action *> reusedPlan(plan.begin(), plan.end());
		
		while (insert_N >= 0) {
			cerr << "insert " << insert_N << " remove " << delete_N << endl;

			stringstream queryStream;

			list<Action *>::const_iterator planIt = reusedPlan.begin();
			
			for(unsigned int timeStep = insert_N; planIt != reusedPlan.end(); ++planIt, ++timeStep) {
				queryStream << (*planIt)->toASP(timeStep) << "." << endl;
			}
						
			queryStream << goalSpecification << endl;
			cerr << queryStream.str() << endl;
			queryStream << "#hide." << endl;
		
			ActionFactory::ActionMap::const_iterator actIt = ActionFactory::actions().begin();
			for( ; actIt != ActionFactory::actions().end(); ++actIt) {
				//the last parameter is always the the step number ???
				queryStream << "#show " << actIt->second->getName() << "/" << actIt->second->paramNumber() + 1 << "." << endl;
			}

			bwi_kr::AnswerSetMsg answerSet = kr_query(queryStream.str(),reusedPlan.size()+insert_N, "repairPlan.asp");
			if (answerSet.satisfied) {
				cerr << "satisfied" << endl;
				vector<bwi_kr::Predicate> &preds = answerSet.predicates;
				vector<Action *> planVector(preds.size());

				for (int j=0 ; j<preds.size() ; ++j) {
		
				Action *act = ActionFactory::byName(preds[j].name);
				act->init(preds[j].parameters);
				planVector[preds[j].timeStep] = act;
				}

				std::list<Action *> repairedPlan(planVector.begin(),planVector.end());
				return repairedPlan;
			}			

			insert_N--;
			delete_N++;

			reusedPlan.pop_front();
		}
	}

	std::list<Action *> repairedPlan;
	return repairedPlan;
}


