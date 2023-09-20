using System;
using UnityEngine;

public class MoveCube : MonoBehaviour
{
    private Vector3 _targetRotation;
    private Vector3 _targetPosition;
    private Vector3 _rotationInc;
    private Vector3 _positionInc;
    private Transform _transform;
    private double _x;
    
    // Start is called before the first frame update
    void Start()
    {
        _transform = transform;
        _targetRotation = _transform.eulerAngles;
        _targetPosition = _transform.position;
    }

    // Update is called once per frame
    void Update()
    {
        _transform.position = MoveCircular(Time.deltaTime);
    }

    Vector3 MoveCircular(double timing)
    {
        _x += timing * 3;
        return new Vector3((float)Math.Sin(_x), (float)Math.Cos(_x), 0);
    }
}
