#pragma once

#include "taichi/backends/device.h"
#include "taichi/backends/dx/dx_info_queue.h"
#include <d3d11.h>

namespace taichi {
namespace lang {
namespace directx11 {

// Only enable debug layer when the corresponding testing facility is enabled
constexpr bool kD3d11DebugEnabled = true;
constexpr bool kD3d11ForceRef = false;  // Force REF device. May be used to
                                        // force software rendering.

void debug_enabled(bool);
void force_ref(bool);
void check_dx_error(HRESULT hr, const char *msg);

class Dx11ResourceBinder : public ResourceBinder {
 public:
  ~Dx11ResourceBinder() override;
  std::unique_ptr<ResourceBinder::Bindings> materialize() override;
  void rw_buffer(uint32_t set,
                 uint32_t binding,
                 DevicePtr ptr,
                 size_t size) override;
  void rw_buffer(uint32_t set,
                 uint32_t binding,
                 DeviceAllocation alloc) override;
  void buffer(uint32_t set,
              uint32_t binding,
              DevicePtr ptr,
              size_t size) override;
  void buffer(uint32_t set, uint32_t binding, DeviceAllocation alloc) override;
  void image(uint32_t set,
             uint32_t binding,
             DeviceAllocation alloc,
             ImageSamplerConfig sampler_config) override;

  // Set vertex buffer (not implemented in compute only device)
  void vertex_buffer(DevicePtr ptr, uint32_t binding = 0) override;

  // Set index buffer (not implemented in compute only device)
  // index_width = 4 -> uint32 index
  // index_width = 2 -> uint16 index
  void index_buffer(DevicePtr ptr, size_t index_width) override;

  const std::unordered_map<uint32_t, uint32_t> &binding_to_alloc_id() {
    return binding_to_alloc_id_;
  }

 private:
  std::unordered_map<uint32_t, uint32_t> binding_to_alloc_id_;
};

class Dx11Device;

class Dx11Pipeline : public Pipeline {
 public:
  Dx11Pipeline(const PipelineSourceDesc &desc,
               const std::string &name,
               Dx11Device *device);
  ~Dx11Pipeline() override;
  ResourceBinder *resource_binder() override;

 private:
  std::shared_ptr<Dx11Device> device_{};
  ID3D11ComputeShader *compute_shader_{};
  Dx11ResourceBinder binder_{};
};

class Dx11Stream : public Stream {
 public:
  Dx11Stream(Dx11Device *);
  ~Dx11Stream() override;

  std::unique_ptr<CommandList> new_command_list() override;
  void submit(CommandList *cmdlist) override;
  void submit_synced(CommandList *cmdlist) override;
  void command_sync() override;

 private:
  Dx11Device *device_;
};

class Dx11CommandList : public CommandList {
 public:
  Dx11CommandList(Dx11Device *ti_device);
  ~Dx11CommandList() override;

  void bind_pipeline(Pipeline *p) override;
  void bind_resources(ResourceBinder *binder) override;
  void bind_resources(ResourceBinder *binder,
                      ResourceBinder::Bindings *bindings) override;
  void buffer_barrier(DevicePtr ptr, size_t size) override;
  void buffer_barrier(DeviceAllocation alloc) override;
  void memory_barrier() override;
  void buffer_copy(DevicePtr dst, DevicePtr src, size_t size) override;
  void buffer_fill(DevicePtr ptr, size_t size, uint32_t data) override;
  void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) override;

  // These are not implemented in compute only device
  void begin_renderpass(int x0,
                        int y0,
                        int x1,
                        int y1,
                        uint32_t num_color_attachments,
                        DeviceAllocation *color_attachments,
                        bool *color_clear,
                        std::vector<float> *clear_colors,
                        DeviceAllocation *depth_attachment,
                        bool depth_clear) override;
  void end_renderpass() override;
  void draw(uint32_t num_verticies, uint32_t start_vertex = 0) override;
  void clear_color(float r, float g, float b, float a) override;
  void set_line_width(float width) override;
  void draw_indexed(uint32_t num_indicies,
                    uint32_t start_vertex = 0,
                    uint32_t start_index = 0) override;
  void image_transition(DeviceAllocation img,
                        ImageLayout old_layout,
                        ImageLayout new_layout) override;
  void buffer_to_image(DeviceAllocation dst_img,
                       DevicePtr src_buf,
                       ImageLayout img_layout,
                       const BufferImageCopyParams &params) override;
  void image_to_buffer(DevicePtr dst_buf,
                       DeviceAllocation src_img,
                       ImageLayout img_layout,
                       const BufferImageCopyParams &params) override;

  void run_commands();

 private:
  struct Cmd {
    explicit Cmd(Dx11CommandList *cmdlist) : cmdlist_(cmdlist) {
    }
    virtual void execute() {
    }
    Dx11CommandList *cmdlist_;
  };

  struct CmdBufferFill : public Cmd {
    explicit CmdBufferFill(Dx11CommandList *cmdlist) : Cmd(cmdlist) {
    }
    ID3D11UnorderedAccessView *uav{nullptr};
    size_t offset{0}, size{0};
    uint32_t data{0};
    void execute() override;
  };

  std::vector<std::unique_ptr<Cmd>> recorded_commands_;
  Dx11Device *device_;
};

class Dx11Device : public GraphicsDevice {
 public:
  Dx11Device();
  ~Dx11Device() override;

  DeviceAllocation allocate_memory(const AllocParams &params) override;
  void dealloc_memory(DeviceAllocation handle) override;
  std::unique_ptr<Pipeline> create_pipeline(
      const PipelineSourceDesc &src,
      std::string name = "Pipeline") override;
  void *map_range(DevicePtr ptr, uint64_t size) override;
  void *map(DeviceAllocation alloc) override;
  void unmap(DevicePtr ptr) override;
  void unmap(DeviceAllocation alloc) override;
  void memcpy_internal(DevicePtr dst, DevicePtr src, uint64_t size) override;
  Stream *get_compute_stream() override;
  std::unique_ptr<Pipeline> create_raster_pipeline(
      const std::vector<PipelineSourceDesc> &src,
      const RasterParams &raster_params,
      const std::vector<VertexInputBinding> &vertex_inputs,
      const std::vector<VertexInputAttribute> &vertex_attrs,
      std::string name = "Pipeline") override;
  Stream *get_graphics_stream() override;
  std::unique_ptr<Surface> create_surface(const SurfaceConfig &config) override;
  DeviceAllocation create_image(const ImageParams &params) override;
  void destroy_image(DeviceAllocation handle) override;

  void image_transition(DeviceAllocation img,
                        ImageLayout old_layout,
                        ImageLayout new_layout) override;
  void buffer_to_image(DeviceAllocation dst_img,
                       DevicePtr src_buf,
                       ImageLayout img_layout,
                       const BufferImageCopyParams &params) override;
  void image_to_buffer(DevicePtr dst_buf,
                       DeviceAllocation src_img,
                       ImageLayout img_layout,
                       const BufferImageCopyParams &params) override;

  int live_dx11_object_count();
  ID3D11DeviceContext *d3d11_context() {
    return context_;
  }

  ID3D11Buffer *alloc_id_to_buffer(uint32_t alloc_id);
  ID3D11Buffer *alloc_id_to_buffer_cpu_copy(uint32_t alloc_id);
  ID3D11UnorderedAccessView *alloc_id_to_uav(uint32_t alloc_id);
  ID3D11Device *d3d11_device() {
    return device_;
  }

 private:
  void create_dx11_device();
  void destroy_dx11_device();
  ID3D11Device *device_{};
  ID3D11DeviceContext *context_{};
  std::unique_ptr<Dx11InfoQueue> info_queue_{};
  std::unordered_map<uint32_t, ID3D11Buffer *>
      alloc_id_to_buffer_;  // binding ID to buffer
  std::unordered_map<uint32_t, ID3D11Buffer *>
      alloc_id_to_cpucopy_;  // binding ID to CPU copy of buffer
  std::unordered_map<uint32_t, ID3D11UnorderedAccessView *>
      alloc_id_to_uav_;  // binding ID to UAV
  int alloc_serial_;
  Dx11Stream *stream_;
};

}  // namespace directx11
}  // namespace lang
}  // namespace taichi
