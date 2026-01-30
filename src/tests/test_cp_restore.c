#include <stdio.h>
#include <stdlib.h>

#include <utils/macros.h>
#include <utils/vector.h>
#include <closure.h>
#include <cont.h>

#define NODE_DATA_SIZE 100
#define NODE_COUNT 3

int delete_existing_container_if_present(){
    char fileName[100];
    strcpy(fileName,FM_FILE_NAME_PREFIX );
    strcat(fileName,"0");
    return remove(fileName);
}

void create_list_cpoint(int num_of_nodes)
{
     delete_existing_container_if_present();
    
    // create an empy container
    struct container *cont = container_init();

    struct Node {
        struct Node *next;
        char data[NODE_DATA_SIZE];
    } *array[num_of_nodes];

    for (int i = 0; i < num_of_nodes; i++) {
        array[i] = malloc(sizeof(struct Node));
        sprintf(array[i]->data, "node: %d", i);

        // manual annotation of pointers and mallocs
        mallocat(array[i], sizeof(struct Node));
        pointerat(cont->id, &array[i]->next);
    }

    for(int i = 0; i< num_of_nodes-1; i++)
         array[i]->next = array[i+1];

    array[num_of_nodes-1]->next = NULL;


    struct Node *head = container_palloc(cont->id, sizeof(*head));
    head->next = array[0];
    sprintf(head->data, "%s", "contianer head!");
    container_setroot(cont->id, head);
    pointerat(cont->id, &head->next);
    container_cpoint(cont->id);

    
    struct Node* node = head;
    while (node) {
        printf("\tnode { addr: %p, data: '%s', next: %p }\n", node, node->data, node->next);
        node = node->next;
    }
}

void test_all()
{

    delete_existing_container_if_present();
    

    // create an empy container
    struct container *cont = container_init();

    struct Node {
        struct Node *next;
        char data[NODE_DATA_SIZE];
    } *array[NODE_COUNT];

    for (int i = 0; i < NODE_COUNT; i++) {
        array[i] = malloc(sizeof(struct Node));
        sprintf(array[i]->data, "node: %d", i);

        // manual annotation of pointers and mallocs
        mallocat(array[i], sizeof(struct Node));
        pointerat(cont->id, &array[i]->next);
    }

    for(int i = 0; i< NODE_COUNT-1; i++)
         array[i]->next = array[i+1];

    array[NODE_COUNT-1]->next = NULL;


    struct Node *head = container_palloc(cont->id, sizeof(*head));
    head->next = array[0];
    sprintf(head->data, "%s", "contianer head!");
    container_setroot(cont->id, head);
    pointerat(cont->id, &head->next);
    container_cpoint(cont->id);

    struct Node *new1, *new2, *new3;
    
    new1 = head->next;
    new2 = new1->next;
    new3 = new2->next;

    new1->next = new3;
    new3->next = new2;
    new2->next = NULL;
    
    pointerat(cont->id, &head->next);
    pointerat(cont->id, &new1->next);
    pointerat(cont->id, &new2->next);
    pointerat(cont->id, &new3->next);

    htable_print();
   
    container_cpoint(cont->id);


    printf("Printing list from the volatile head\n");
    struct Node *node = array[0];
    while (node) {
        printf("\tnode { addr: %p, data: '%s', next: %p }\n", node, node->data, node->next);
        node = node->next;
    }

    printf("Printing list from the container\n");
    node = head;
    while (node) {
        printf("\tnode { addr: %p, data: '%s', next: %p }\n", node, node->data, node->next);
        node = node->next;
    }

}

int main(int argc, const char *argv[])
{

    char option;
 /*   while(1)
    {
        printf("input option:");
        if(option == 'c')
        {
            printf("input number of nodes:");
            option = getchar();
            create_list_cpoint(atoi(5));
        }

    }

*/
    return 0;
}
