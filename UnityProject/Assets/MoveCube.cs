using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MoveCube : MonoBehaviour
{
    private Vector3 _targetRotation;
    private Vector3 _targetPosition;
    private Vector3 _rotationInc;
    private Vector3 _positionInc;
    
    // Start is called before the first frame update
    void Start()
    {
        _targetRotation = transform.eulerAngles;
        _targetPosition = transform.position;
    }

    // Update is called once per frame
    void Update()
    {
        // if (_targetRotation == transform.eulerAngles)
        // {
        //     _targetRotation = new Vector3(Random.value, Random.value, Random.value) * 360;
        //     _rotationInc = (_targetRotation - transform.eulerAngles) / 2;
        // }
        //
        // if (_targetPosition == transform.position)
        // {
        //     _targetPosition = new Vector3(Random.value - 0.5f, Random.value - 0.5f, Random.value - 0.5f) * 10;
        //     _positionInc = (_targetPosition - transform.position) / 2;
        // }
        //
        // transform.position += _positionInc;
        // transform.eulerAngles += _rotationInc;
    }
}
