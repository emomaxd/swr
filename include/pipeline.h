#ifndef PIPELINE_H
#define PIPELINE_H

#include <cstring>

#include "core.h"

struct Pipeline
{
  u32* Positions;
  u32* Colors;
  u32  Count;
  u32  Length;
};

void InitPipeline(Pipeline* pipeline)
{
  const u32 size      = sizeof(u32);
  const u32 length    = 1024;
  pipeline->Positions = (u32 *)malloc(size * length);
  pipeline->Colors    = (u32 *)malloc(size * length); /* Pre-allocate 1024 vertices */
  pipeline->Length    = length;
  pipeline->Count     = 0;
}

static void AddHelper(Pipeline* pipeline, u32* pos_arr, u32 size)
{
  u32 *add_position = (u32 *)(pipeline->Positions + sizeof(u32) * pipeline->Count);
  memcpy(add_position, pos_arr, size);
}

static void Resize(Pipeline* pipeline, u32* dont_use, u32 new_size)
{
  u32* arr = pipeline->Positions;
  u32 size = pipeline->Length * sizeof(u32);
  u32* temp = (u32 *)malloc(new_size);
  memcpy(temp, arr, size);
  free(arr);
  arr = temp;
}

typedef void (*funptr)(Pipeline*, u32*, u32);
void AddPosition(Pipeline* pipeline, u32* pos_arr, u32 size)
{
  static funptr functions[] = {
    Resize,
    AddHelper
  };

  u32 index =  (u32)(pipeline->Length - pipeline->Count + size);
  funptr selected_function = functions[index];
  selected_function(pipeline, pos_arr, size);
}


#endif // !PIPELINE_H
