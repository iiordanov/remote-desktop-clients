package com.iiordanov.bVNC;

import android.app.Activity;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.tasks.Task;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;

public class GoogleUtils {
    private final static String TAG = "GoogleUtils";

    public static void showRateAppDialog(Activity activity) {
        ReviewManager manager = ReviewManagerFactory.create(activity.getApplicationContext());
        Task<ReviewInfo> request = manager.requestReviewFlow();
        request.addOnCompleteListener(task -> {
            GoogleApiAvailability apiAvailability = GoogleApiAvailability.getInstance();
            if (apiAvailability.isGooglePlayServicesAvailable(activity) == ConnectionResult.SUCCESS) {
                if (task.isSuccessful()) {
                    ReviewInfo reviewInfo = task.getResult();
                    Task<Void> flow = manager.launchReviewFlow(activity, reviewInfo);
                    flow.addOnCompleteListener(completedTask -> {
                        Log.d(TAG, "rateApp: Completed: " + completedTask.getResult());
                    });
                } else {
                    Log.d(TAG, "rateApp: task is not successful");
                }
            }
        });
    }
}
