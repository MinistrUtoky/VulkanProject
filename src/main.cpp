#include <engine.h>
#include <database_controller.h>

int main(int argc, char* argv[])
{
	DataController dataController;
	dataController.retrieve_all_blobs();
	VulkanEngine engine;
	engine.init();
	engine.run();
	engine.cleanup();	
	return 0;
}
