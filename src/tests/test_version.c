#include <stdio.h>
#include <stdlib.h>
#include <cont.h>
#include <closure.h>
#include <container_version.h>

#define NODE_DATA_SIZE 6000
#define NODE_COUNT 10
#define CONTAINER_NUMBER 0
#define DATA_POINT_TO_PRINT 8
#define ASYNC_CP 0
#define SLEEP 0


unsigned long long counter = 1;
int temp;

int print = 1;
 struct Node {
        struct Node *next;
         unsigned long long data[NODE_DATA_SIZE];
       
    };

int delete_container(){
    char fileName[100];
    strcpy(fileName,FM_FILE_NAME_PREFIX );
    strcat(fileName,"0");
    return remove(fileName);
}

void print_nodes(struct Node* node, int type)
{
    if(!print)
        return;
    if(type == 0)
        printf("Nodes in Volatile Memory:\n");
    else
        printf("Nodes in Persistent Memory:\n");
    int count = 0;
    while (node) {
        count++;
        int data_to_print = 
        printf("\t%d. node { addr: %p,data: %d %d %d %d, next: %p }\n",
        count, node, node->data[0], node->data[NODE_DATA_SIZE/4], node->data[NODE_DATA_SIZE/2],
        node->data[NODE_DATA_SIZE-1],node->next);
        node = node->next;
    }

}

void create_list_and_cpoint()
{
    printf("Creating list:\n");
    
    struct Node* array[NODE_COUNT];
    for (int i = 0; i < NODE_COUNT; i++) {
        array[i] = malloc(sizeof(struct Node));
        for(int j = 0; j< NODE_DATA_SIZE; j++)
            array[i]->data[j] = 0;
        array[i]->next = 0;
    }
    for(int i = 0; i< NODE_COUNT-1; i++)
         array[i]->next = array[i+1];
    array[NODE_COUNT-1]->next = NULL;
    print_nodes(array[0],0);


    delete_container();
     struct container *cont = container_init();
    // Manual insertion of pointers. Will not be required in CIL integrated version.
     for (int i = 0; i < NODE_COUNT; i++) {
        mallocat(array[i], sizeof(struct Node));
        pointerat(cont->id, &array[i]->next);
     }

    struct Node *head = container_palloc(cont->id, sizeof(*head));
    head->next = array[0];
    for(int j = 0; j< NODE_DATA_SIZE; j++)
            head->data[j] = 0;
    container_setroot(cont->id, head);
    pointerat(cont->id, &head->next); // Will not be required in CIL integrated version.
    
    int allocated_pages = container_checkpoint_with_phase(cont->id,ASYNC_CP,&temp);
    if(SLEEP)
        sleep(SLEEP);
    if(print)
    {
        printf("allocated pages: %d\n",allocated_pages);
        container_pprint_all_versions();
        print_nodes(head,1);
        
    }
   
}

void restore_and_add_to_list_and_cpoint()
{
    printf("add nodes to list:\n");

    struct container *cont = container_restore(CONTAINER_NUMBER);

    int node_counter;       
  
    struct Node* array[NODE_COUNT]; 
    for (int i = 0; i < NODE_COUNT; i++) {
        array[i] = malloc(sizeof(struct Node));
        for(int j = 0; j< NODE_DATA_SIZE; j++)
            array[i]->data[j] = counter;
    }
    for(int i = 0; i< NODE_COUNT-1; i++)
         array[i]->next = array[i+1];
    array[NODE_COUNT-1]->next = NULL;
    print_nodes(array[0],0);
    
    // Manual insertion of pointers. Will not be required in CIL integrated version.
    for (int i = 0; i < NODE_COUNT; i++) {
        mallocat(array[i], sizeof(struct Node));
        pointerat(cont->id, &array[i]->next);
    
    }
    
    struct Node* node = container_getroot(CONTAINER_NUMBER);
    while (node && node->next)
         node = node->next;
    node->next = array[0];
    pointerat(cont->id, &node->next);// Will not be required in CIL integrated version.
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
    int allocated_pages = container_checkpoint(cont->id,0);

    if(print)
    {
        printf("allocated pages: %d\n",allocated_pages);
        container_pprint_all_versions();
        print_nodes(container_getroot(CONTAINER_NUMBER),1);

    }
    
    counter++;
}
void restore_list()
{
    printf("Restoring list:\n");
    struct container *cont;
    cont = container_restore(CONTAINER_NUMBER);
    if(print)
    {
         container_pprint_all_versions();
        print_nodes( container_getroot(CONTAINER_NUMBER),1);

    }
   
}

void restore_all_versions()
{
    struct container *cont;

    struct version_dir *vd = get_version_dir(CONTAINER_NUMBER);

    uint32_t version_count = cont->version_dir_index;
    int current_version_dir_index = 1;
    uint32_t pos = 0;

    for(int i = 0; i < version_count;i++)
    {
        if(i >= VERSION_ENTRIES*current_version_dir_index)
        {
            vd = vd->next_current_version_dir.maddr;
            pos = 0;
        }
        
        printf("Restoring version: %d\n",vd->version_entries[pos].version_id);
        cont = container_restore_for_version(CONTAINER_NUMBER,vd->version_entries[pos].version_id);
        print_nodes( container_getroot(CONTAINER_NUMBER),1);
        
    }
}

void restore_and_modify_pointers_and_checkpoint()
{
    printf("Restoring and Modifying pointers:\n");

    struct container *cont;
    cont = container_restore(CONTAINER_NUMBER);
    struct Node* node =  container_getroot(CONTAINER_NUMBER);
    while(node && node->next)
    {
        node->next = node->next->next;
        pointerat(cont->id, &node->next);
        node = node->next;
    }
    int allocated_pages = container_checkpoint_with_phase(cont->id,ASYNC_CP,&temp);

    if(print)
    {

        printf("allocated pages: %d\n",allocated_pages);
        container_pprint_all_versions();
        print_nodes( container_getroot(CONTAINER_NUMBER),1);
    }
   
}

void restore_and_modify_values_and_checkpoint()
{
    if(print)
    {
        printf("Restoring and Modifying pointers:\n");
    }

    struct container *cont;
    cont = container_restore(CONTAINER_NUMBER);
    struct Node* node =  container_getroot(CONTAINER_NUMBER);
    while(node)
    {
        for(int j = 0; j< NODE_DATA_SIZE; j++)
            node->data[j] = rand()%1000;
        node = node->next;
    }
    if(SLEEP)
        sleep(SLEEP);
    if(print)
    {
        int allocated_pages = container_cpoint(cont->id);
        printf("allocated pages: %d\n",allocated_pages);
        container_pprint_all_versions();
        print_nodes( container_getroot(CONTAINER_NUMBER),1);

    }
   
    counter++;
}


int main(int argc, const char *argv[])
{ 
    create_list_and_cpoint();;
    for(int i = 0; i < 500; i++)
    {
        restore_and_add_to_list_and_cpoint();
    }
        return;

       
    while(1)
    {
        char option;
        option = getchar();
        switch (option)
        {
        case 'c':
            create_list_and_cpoint();
            break;
        case 'r':
            restore_list();
            break; 
        case 'v':
            restore_all_versions();
            break;      
        case 'a':
             restore_and_add_to_list_and_cpoint();
             break;
        case 'p':
            restore_and_modify_pointers_and_checkpoint();
            break;
        case 'x':
            restore_and_modify_values_and_checkpoint();
            
            break;
        case 'q':
            return 0;
        default:
            break;
        }
    }
    return 0;
}
