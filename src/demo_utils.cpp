#include "demo.h"
namespace jsr {
    namespace utils {
        void printVector(const glm::vec4& v) {
            printf("%+.4f  %+.4f  %+.4f  %+.4f\n",
                v.x,
                v.y,
                v.z,
                v.w);
        }

        bool pointInFrustum(const glm::vec4& p)
        {
            return
                -p.w < p.x && p.w > p.x &&
                -p.w < p.y && p.w > p.y &&
                0.0f < p.z && p.w > p.z;
        }

        bool pointInFrustum(const glm::vec4& p, const glm::vec4 planes[6])
        {
            float distance = 0.0;
            for (int j = 0; j < 6; j++) {
                distance = dot(p, planes[j]);

                // If one of the tests fails, then there is no intersection
                if (distance <= 0.0) {
                    printf("plane test failed: %d (%f)\n", j, distance);
                    break;
                }

            }
            return distance > 0.0;
        }
    }
}