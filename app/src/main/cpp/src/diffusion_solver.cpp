#include "diffusion_solver.h"

int DiffusionSolver::load(const string &path) {
    int res = 0;

    res = uNet.load(path);
    if (res < 0) {
        LOGE("uNet load fail!");
        return res;
    }
    LOGI("DiffusionSolver load success ,path= %s",path.c_str());
    return res;
}

int DiffusionSolver::init_scheduler()  {
    int res = 0;
    scheduler = new scheduler_dpmpp_2m();
    LOGI("DiffusionSolver init success with scheduler");
    return res;
}

void DiffusionSolver::set_latent_size(int latent_size_h, int latent_size_w) {
    LOGI("DiffusionSolver set latent size %d(h) x %d(w)", latent_size_h, latent_size_w);

    latent_h = latent_size_h;
    latent_w = latent_size_w;
    input_sample_size = 1 * latent_c * latent_h * latent_w;
    uNet.set_latent_size(latent_size_h, latent_size_w);
}


int DiffusionSolver::CFGDenoiser_CompVisDenoiser(cv::Mat &input, float t,
                                                 const cv::Mat &cond, const cv::Mat &uncond,
                                                 cv::Mat &denoised) {

    cv::Mat denoised_cond(cv::Size(latent_w, latent_h), CV_32FC4);
    cv::Mat denoised_uncond(cv::Size(latent_w, latent_h), CV_32FC4);
    if (uNet.denoise(input, t, cond, denoised_cond)<0) {
        return -1;
    }
    if (uNet.denoise(input, t, uncond, denoised_uncond)<0) {
        return -1;
    }

    auto *u_ptr = reinterpret_cast<float *>(denoised_uncond.data);
    auto *c_ptr = reinterpret_cast<float *>(denoised_cond.data);
    for (int hwc = 0; hwc < input_sample_size; hwc++) {
        (*u_ptr) = (*u_ptr) + guidance_scale * ((*c_ptr) - (*u_ptr));
        u_ptr++;
        c_ptr++;
    }

    denoised = denoised_uncond.clone();
    return 0;
}

int DiffusionSolver::sampler_txt2img(int seed, int step, cv::Mat &c, cv::Mat &uc, cv::Mat &x_mat) {
    init_scheduler();

    uNet.before_run();
    // init
    auto timesteps = scheduler->set_timesteps(step);
    x_mat = scheduler->randn_mat(seed % 100, latent_h, latent_w, 1); //generateLatentSample
    cv::Mat old_noised(cv::Size(latent_h, latent_w), CV_32FC4);

    LOGI("sampler_txt2img,step = %2d,timesteps.size()= : %2zu ",step,timesteps.size() );

    for (int i = 0; i < timesteps.size(); i++) {
        auto t1 = std::chrono::high_resolution_clock::now();
        cv::Mat latent_input = scheduler->scale_model_input(x_mat, i);  //latentModelInput

        cv::Mat denoised;
        if (CFGDenoiser_CompVisDenoiser(latent_input, (float)i, c, uc, denoised) < 0) {
            return -1;
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        LOGI("sampler_txt2img,scheduler->step before= : %2d ",i );
        x_mat = scheduler->step(i, x_mat, denoised, old_noised);
        LOGI("sampler_txt2img,scheduler->step after= : %2d ",i );
        auto t3 = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
        auto elapsedTime1 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
        LOGI("Step: %2d / %lu  | %fms, %fms", i + 1, step, (float) (elapsedTime.count() / 1000),
             (float) (elapsedTime1.count() / 1000));
    }
    uNet.after_run();
    return 0;
}

int DiffusionSolver::unload() {
    return uNet.unload();
}



