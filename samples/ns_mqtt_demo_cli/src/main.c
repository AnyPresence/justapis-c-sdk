//
//  main.c
//  NSMQTTDemoCLI
//
//  Created by Taha Samad on 2/9/17.
//  Copyright © 2017 Nanoscale. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "justapis.h"
#import <mosquitto.h>
#import <pthread.h>

/// Configurations
const char* host = "127.0.0.1";
const char* username = "developer@example.net,PushNotificationAPI,pushRemoteEndpoint,mqtt";
const char* password = "shared123";
const char* subscribeTopic = "/user/2/";
const char* publishTopic = "/user/1/";

const int subscribeQoS = 2;
const int publishQoS = 2;
const int connectQoS = 2;

//Instructions

const char* toggle_conn_instruction = "toggle_connection";
const char* toggle_sub_instruction = "toggle_subscription";
const char* publish_instruction = "publish";
const char* exit_instruction = "exit";
const char* refresh_instruction = "<Enter> to Refresh";

//State Vars
bool connected = false;
bool subscribed = false;
ja_mqtt_connection *current_connection = NULL;
ja_simple_buffer* received_messages = NULL;
ja_simple_buffer* published_messages = NULL;

char* current_status_message = NULL;
char* current_instruction_message = NULL;
bool is_custom_instruction_message = false;
ja_simple_buffer* current_input_string = NULL;

//Thread Safety

pthread_mutex_t main_mutex;

/// Helping Functions

char* message_buffer_new()
{
    return malloc(sizeof(char) * 1024);
}

void print_current_instruction_message_and_input_string()
{
    if (current_instruction_message != NULL) {
        printf("%s: ", current_instruction_message);
    }
    if (current_input_string != NULL && current_input_string->data_length > 0)
    {
        printf("%s", current_input_string->data);
    }
}

void print_status()
{
    printf("\n\n");
    printf("---------------------\n");
    printf("\n");
    
    printf("%s\n", host);
    printf("Connected: %d\n", connected);
    printf("Subscribed to '%s': %d\n", subscribeTopic, subscribed);
    printf("\n");
    
    printf("Received Messages:\n");
    printf("\n");
    if (received_messages != NULL && received_messages->data_length > 0)
    {
        printf("%s", received_messages->data);
    }
    else
    {
        printf("---<none>---\n");
    }
    printf("\n");
    
    printf("Published Messages To '%s':\n", publishTopic);
    printf("\n");
    if (published_messages != NULL && published_messages->data_length > 0)
    {
        printf("%s", published_messages->data);
    }
    else
    {
        printf("---<none>---\n");
    }
    printf("\n");
    
    if (current_status_message != NULL)
    {
        printf("%s", current_status_message);
        printf("\n");
    }
}

void set_new_status_message(char* new_status_message)
{
    if(current_status_message != NULL)
    {
        free(current_status_message);
    }
    current_status_message = new_status_message;
}

void current_input_string_free()
{
    if (current_input_string != NULL)
    {
        ja_simple_buffer_free(current_input_string);
        current_input_string = NULL;
    }
}

void setup_current_instruction_message(char* custom_instruction_message, bool retain_old_custom_instruction_message_if_connected)
{
    if (retain_old_custom_instruction_message_if_connected == true && is_custom_instruction_message == true && connected == true)
    {
        return;
    }
    if (current_instruction_message != NULL)
    {
        free(current_instruction_message);
        current_instruction_message = NULL;
    }
    char* instruction_message = NULL;
    if (custom_instruction_message != NULL)
    {
        instruction_message = ja_str_copy(custom_instruction_message);
        is_custom_instruction_message = true;
    }
    else
    {
        instruction_message = message_buffer_new();
        if (connected)
        {
            sprintf(instruction_message , "Enter an instruction(%s, %s, %s, %s, %s)", toggle_conn_instruction, toggle_sub_instruction, publish_instruction, exit_instruction, refresh_instruction);
        }
        else
        {
            sprintf(instruction_message , "Enter an instruction(%s, %s, %s)", toggle_conn_instruction, exit_instruction, refresh_instruction);
        }
        is_custom_instruction_message = false;
    }
    current_instruction_message = instruction_message;
}

void refresh_status()
{
    print_status();
    setup_current_instruction_message(NULL, true);
    print_current_instruction_message_and_input_string();
}

void getInput()
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    current_input_string_free();
    current_input_string = ja_simple_buffer_append(NULL, NULL, 0);
    print_current_instruction_message_and_input_string();
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
    while(true)
    {
        char x = getchar();
        if(x == '\n')
        {
            break;
        }
        else if (x >= 32 && x <= 127)//Visible Range
        {
            //@Lock
            pthread_mutex_lock(&main_mutex);
            //----
            current_input_string = ja_simple_buffer_append(current_input_string, &x, 1);
            //@Unlock
            pthread_mutex_unlock(&main_mutex);
            //----
        }
    }
}

// MQTT Callbacks:

//Callbacks are on a different thread.

void on_connect(ja_mqtt_connection* connection, int error)
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (connection == current_connection)
    {
        //New Status
        char* status_msg = message_buffer_new();
        if (error == ja_mqtt_error_success)
        {
            connected = true;
            sprintf(status_msg , "Connected\n");
        }
        else
        {
            sprintf(status_msg, "Failed to connect due to error: %d -> %s\n", error, mosquitto_strerror(error));
        }
        set_new_status_message(status_msg);
        //Refresh
        refresh_status();
    }
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
}

void on_disconnect(ja_mqtt_connection* connection, int error)
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (connection == current_connection)
    {
        //New Status
        char* status_msg = message_buffer_new();
        connected = false;
        subscribed = false;
        ja_mqtt_connect(current_connection);
        current_connection = NULL;
        if (received_messages != NULL)
        {
            ja_simple_buffer_free(received_messages);
        }
        received_messages = ja_simple_buffer_append(NULL, NULL, 0);
        if (published_messages != NULL)
        {
            ja_simple_buffer_free(published_messages);
        }
        published_messages = ja_simple_buffer_append(NULL, NULL, 0);
        sprintf(status_msg, "Disconnected with to error: %d -> %s\n", error, mosquitto_strerror(error));
        set_new_status_message(status_msg);
        //Refresh
        refresh_status();
    }
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
}

void on_subscribe(ja_mqtt_connection* connection, int mid, const int* granted_qos, int granted_qos_count)
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (connection == current_connection)
    {
        //New Status
        char* status_msg = message_buffer_new();
        subscribed = granted_qos_count > 0;
        //Status Message
        char* qos_string = message_buffer_new();
        ja_simple_buffer *granted_qos_string = ja_simple_buffer_append(NULL, NULL, 0);
        for (int i = 0; i < granted_qos_count; i++)
        {
            sprintf(qos_string , "%d ", granted_qos[i]);
            granted_qos_string = ja_simple_buffer_append(granted_qos_string, qos_string, strlen(qos_string));
        }
        sprintf(status_msg , "mid# %d -> Subscribe ACK with Granted QoS: %s\n", mid, granted_qos_string->data);
        free(qos_string);
        ja_simple_buffer_free(granted_qos_string);
        set_new_status_message(status_msg);
        //Refresh
        refresh_status();
    }
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
}

void on_unsubscribe(ja_mqtt_connection* connection, int mid)
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (connection == current_connection)
    {
        //New Status
        char* status_msg = message_buffer_new();
        //Status Message
        sprintf(status_msg , "mid# %d -> Unsubscribe ACK\n", mid);
        set_new_status_message(status_msg);
        //Refresh
        refresh_status();
    }
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
}

void on_publish(ja_mqtt_connection* connection, int mid)
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (connection == current_connection)
    {
        //New Status
        char* status_msg = message_buffer_new();
        //Status Message
        sprintf(status_msg , "mid# %d -> Publish ACK\n", mid);
        set_new_status_message(status_msg);
        //Refresh
        refresh_status();
    }
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
}

void on_message(ja_mqtt_connection* connection, ja_mqtt_message* message)
{
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (connection == current_connection)
    {
        char* mid_prefix = message_buffer_new();
        sprintf(mid_prefix , "mid# %d -> ", message->mid);
        //Received Messages
        received_messages = ja_simple_buffer_append(received_messages, mid_prefix, strlen(mid_prefix));
        received_messages = ja_simple_buffer_append(received_messages, message->payload->data, message->payload->data_length);
        received_messages = ja_simple_buffer_append(received_messages, "\n", 1);
        //Status Message
        char* status_msg = message_buffer_new();
        sprintf(status_msg , "mid# %d -> Received Message\n", message->mid);
        set_new_status_message(status_msg);
        //Refresh
        refresh_status();
    }
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //----
}

// MQTT Actions:
void connect_to_broker()
{
    bool connectSuccess = false;
    bool loopStartSuccess = false;
    //Configuration Struct
    ja_mqtt_configuration* config = ja_mqtt_configuration_default(host, username, password);
    config->on_connect_callback = on_connect;
    config->on_disconnect_callback = on_disconnect;
    config->on_subscribe_callback = on_subscribe;
    config->on_unsubscribe_callback = on_unsubscribe;
    config->on_publish_callback = on_publish;
    config->on_message_callback = on_message;
    int error = 0;
    //Connection
    ja_mqtt_connection *connection = ja_mqtt_connection_init(config, &error);
    current_connection = connection;
    char *status_msg = message_buffer_new();
    if (error != ja_mqtt_error_success || connection == NULL)
    {
        sprintf(status_msg, "Failed to instantiate connection due to error: %d -> %s\n", error, mosquitto_strerror(error));
    }
    else {
        error = ja_mqtt_connect(connection);
        if (error != ja_mqtt_error_success)
        {
            sprintf(status_msg, "Failed to connect due to error: %d -> %s\n", error, mosquitto_strerror(error));
        }
        else {
            connectSuccess = true;
            error = ja_mqtt_loop_start(connection);
            if (error != ja_mqtt_error_success)
            {
                sprintf(status_msg, "Failed to start loop due to error: %d -> %s\n", error, mosquitto_strerror(error));
            }
            else  {
                loopStartSuccess = true;
                sprintf(status_msg, "Connecting...\n");
            }
        }
    }
    if (!loopStartSuccess)
    {
        if (connectSuccess)
        {
            ja_mqtt_disconnect(connection);//Undo Connect
        }
        if (connection != NULL)
        {
            ja_mqtt_connection_free(connection);
            current_connection = NULL;
        }
    }
    set_new_status_message(status_msg);
    ja_mqtt_configuration_free(config);//Connection has a deep copy. So we need not worry.
}

void disconnect_from_broker()
{
    char *status_msg = message_buffer_new();
    int error = ja_mqtt_disconnect(current_connection);
    if (error != ja_mqtt_error_success)
    {
        sprintf(status_msg, "Failed to disconnect due to error: %d -> %s\n", error, mosquitto_strerror(error));
    }
    else
    {
        sprintf(status_msg, "Disconnecting...\n");
    }
    set_new_status_message(status_msg);
}

void subscribe_to_topic()
{
    char *status_msg = message_buffer_new();
    int mid = 0;
    int error = ja_mqtt_subscribe(current_connection, subscribeTopic, subscribeQoS, &mid);
    if (error != ja_mqtt_error_success)
    {
        sprintf(status_msg, "Failed to subscribe due to error: %d -> %s\n", error, mosquitto_strerror(error));
    }
    else
    {
        sprintf(status_msg, "Subscribing with mid: %d\n", mid);
    }
    set_new_status_message(status_msg);
}

void unsubscribe_from_topic()
{
    char *status_msg = message_buffer_new();
    int mid = 0;
    int error = ja_mqtt_unsubscribe(current_connection, subscribeTopic, &mid);
    if (error != ja_mqtt_error_success)
    {
        sprintf(status_msg, "Failed to unsubscribe due to error: %d -> %s\n", error, mosquitto_strerror(error));
    }
    else
    {
        sprintf(status_msg, "Unsubscribing with mid: %d\n", mid);
    }
    set_new_status_message(status_msg);
}

void publish_message(char* message)
{
    char *status_msg = message_buffer_new();
    if (connected == true)
    {
        int mid = 0;
        ja_simple_buffer* payload = ja_simple_buffer_append(NULL, message, strlen(message));
        int error = ja_mqtt_publish(current_connection, publishTopic, payload, publishQoS, false, &mid);
        if (error != ja_mqtt_error_success)
        {
            sprintf(status_msg, "Failed to publish due to error: %d -> %s\n", error, mosquitto_strerror(error));
        }
        else
        {
            char *mid_prefix = message_buffer_new();
            sprintf(mid_prefix, "mid# %d -> ", mid);
            published_messages = ja_simple_buffer_append(published_messages, mid_prefix, strlen(mid_prefix));
            free(mid_prefix);
            published_messages = ja_simple_buffer_append(published_messages, payload->data, payload->data_length);
            published_messages = ja_simple_buffer_append(published_messages, "\n", 1);
            sprintf(status_msg, "Publishing with mid: %d\n", mid);
        }
        ja_simple_buffer_free(payload);
    }
    else {
        sprintf(status_msg, "Invalid state to publish\n");
    }
    set_new_status_message(status_msg);
}

bool ask_for_instruction()
{
    bool retValue = true;
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //
    setup_current_instruction_message(NULL, false);
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //
    getInput();
    //@Lock
    pthread_mutex_lock(&main_mutex);
    //----
    if (strcmp(current_input_string->data, toggle_conn_instruction) == 0)
    {
        if (connected)
        {
            disconnect_from_broker();
        }
        else
        {
            connect_to_broker();
        }
    }
    else if (connected && strcmp(current_input_string->data, toggle_sub_instruction) == 0)
    {
        if (subscribed)
        {
            unsubscribe_from_topic();
        }
        else
        {
            subscribe_to_topic();
        }
    }
    else if (connected && strcmp(current_input_string->data, publish_instruction) == 0)
    {
        setup_current_instruction_message("Enter a message", false);
        //@Unlock
        pthread_mutex_unlock(&main_mutex);
        //----
        getInput();
        //@Lock
        pthread_mutex_lock(&main_mutex);
        //----
        publish_message(current_input_string->data);
    }
    else if (strcmp(current_input_string->data, exit_instruction) == 0)
    {
        retValue = false;
    }
    else if (strcmp(current_input_string->data, "") == 0)//Refresh
    {
        char* status_msg = message_buffer_new();
        sprintf(status_msg , "");
        set_new_status_message(status_msg);
    }
    else {
        char* status_msg = message_buffer_new();
        sprintf(status_msg , "Invalid Instruction: %s\n", current_input_string->data);
        set_new_status_message(status_msg);
    }
    // Free memory before proceeding to next step.
    current_input_string_free();
    //@Unlock
    pthread_mutex_unlock(&main_mutex);
    //
    return retValue;
}

/// Main

int main(int argc, const char * argv[])
{
    pthread_mutexattr_t main_mutex_attr;
    pthread_mutexattr_init(&main_mutex_attr);
    pthread_mutexattr_settype(&main_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&main_mutex, &main_mutex_attr);
    
    printf("Hello! Lets Do Some MQTT Messaging.\n");
    do {
        //@Lock
        pthread_mutex_lock(&main_mutex);
        //----
        print_status();
        //@Unlock
        pthread_mutex_unlock(&main_mutex);
        //----
    } while (ask_for_instruction());
    return 0;
}
